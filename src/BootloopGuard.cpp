#include "BootloopGuard.h"
#include "Logger.h"
#include <Preferences.h>
#include <esp_image_format.h>

// 2-byte RTC memory storage placed in .rtc_noinit section.
// This memory retains its content across software restarts, crashes, watchdogs,
// and brownouts.
RTC_NOINIT_ATTR static BootloopGuard::BootCountStorage s_rtc_bootcount;

uint32_t BootloopGuard::s_start_time = 0;
bool BootloopGuard::s_validated = false;

static const char *NVS_NAMESPACE = "bootguard";
static const char *NVS_KEY_TRIAL = "trial";
static const uint8_t MAX_BOOT_ATTEMPTS = 10;
static const uint32_t STABILITY_WINDOW_MS = 60000; // 60 seconds (1 full minute)

// Global C-linkage override for the Arduino core weak hook defined in
// esp32-hal-misc.c. Executes at the very start of initArduino() in app_main(),
// before CPU/PSRAM/NVS init and before setup().
extern "C" bool verifyRollbackLater() {
  BootloopGuard::onBoot();
  return true;
}

// Find alternate app partition, optionally verifying its binary integrity
// (inspired by ESPHome safe_mode)
static const esp_partition_t *findAlternateAppPartition(bool verify) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *result = nullptr;
  esp_partition_iterator_t it = esp_partition_find(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  while (it != nullptr) {
    const esp_partition_t *p = esp_partition_get(it);
    if (p != nullptr && running != nullptr && p->address != running->address) {
      if (!verify) {
        result = p;
        break;
      }
      esp_image_metadata_t data = {};
      const esp_partition_pos_t part_pos = {
          .offset = p->address,
          .size = p->size,
      };
      if (esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &part_pos, &data) ==
          ESP_OK) {
        result = p;
        break;
      }
    }
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
  return result;
}

uint8_t BootloopGuard::readNvsTrialFlags() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {
    return 0;
  }
  uint8_t flags = prefs.getUChar(NVS_KEY_TRIAL, 0);
  prefs.end();
  return flags;
}

void BootloopGuard::writeNvsTrialFlags(uint8_t flags) {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    prefs.putUChar(NVS_KEY_TRIAL, flags);
    prefs.end();
  }
}

int BootloopGuard::getBankIndex(const esp_partition_t *partition) {
  if (!partition)
    return -1;
  if (partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0)
    return 0;
  if (partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1)
    return 1;
  return -1;
}

bool BootloopGuard::isBankInTrial(int bank) {
  if (bank < 0 || bank > 1)
    return false;
  uint8_t flags = readNvsTrialFlags();
  return (flags & (1 << bank)) != 0;
}

void BootloopGuard::setBankTrial(int bank, bool inTrial) {
  if (bank < 0 || bank > 1)
    return;
  uint8_t flags = readNvsTrialFlags();
  if (inTrial) {
    flags |= (1 << bank);
  } else {
    flags &= ~(1 << bank);
  }
  writeNvsTrialFlags(flags);
}

bool BootloopGuard::isRunningInTrial() {
  if (s_validated)
    return false;
  const esp_partition_t *running = esp_ota_get_running_partition();
  int bank = getBankIndex(running);
  return (bank >= 0) ? isBankInTrial(bank) : false;
}

uint8_t BootloopGuard::getBootCount() {
  if (!isRunningInTrial())
    return 0;
  return s_rtc_bootcount;
}

void BootloopGuard::markOtaTrialPending() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  const esp_partition_t *target =
      (boot && boot != running) ? boot
                                : esp_ota_get_next_update_partition(running);

  int targetBank = getBankIndex(target);
  if (targetBank >= 0) {
    setBankTrial(targetBank, true);
    s_rtc_bootcount = 0;
    logger.infof("[BootloopGuard] OTA update finished. Bank %d (%s) set to "
                 "TRIAL mode. Boot count reset to 0.",
                 targetBank, target ? target->label : "ota");
  } else {
    logger.warning("[BootloopGuard] Could not determine OTA target bank to set "
                   "trial flag.");
  }
}

void BootloopGuard::onSdUpdate() {
  markOtaTrialPending();
  logger.info("[BootloopGuard] SD firmware flash detected: trial mode set on "
              "target partition and boot count reset.");
}

void BootloopGuard::onGracefulShutdown() {
  // If the device is being restarted gracefully (e.g. maintenance or user
  // request), clear the crash count so orderly restarts do not trigger a
  // false-positive rollback.
  s_rtc_bootcount = 0;
}

void BootloopGuard::markFirmwareValid() {
  s_validated = true;
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  int bank = getBankIndex(running);

  if (bank >= 0 && isBankInTrial(bank)) {
    setBankTrial(bank, false);
    s_rtc_bootcount = 0;
    logger.infof("[BootloopGuard] Firmware bank %d ran stably for %u seconds! "
                 "Declared VALID, trial flag dropped.",
                 bank, STABILITY_WINDOW_MS / 1000);
  }

  // Only confirm app image in ESP-IDF otadata if running == boot partition
  // (matching ESPHome)
  if (running && boot && running->address == boot->address) {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
      logger.info("[BootloopGuard] App marked valid and rollback cancelled in "
                  "ESP-IDF otadata.");
    }
  }
}

void BootloopGuard::cancelBootloaderRollback() {
  static bool s_rollback_cancelled = false;
  if (s_rollback_cancelled) {
    return;
  }
  s_rollback_cancelled = true;

  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  if (running && boot && running->address == boot->address) {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
      logger.info("[BootloopGuard] First loop pass complete: bootloader "
                  "rollback cancelled.");
    }
  }
}

void BootloopGuard::handle() {
  if (s_validated) {
    return;
  }

  if (!isRunningInTrial()) {
    s_validated = true;
    return;
  }

  if (millis() - s_start_time >= STABILITY_WINDOW_MS) {
    markFirmwareValid();
  }
}

void BootloopGuard::onBoot() {
  // Increment non-wiped RTC RAM boot count as early as possible on boot
  ++s_rtc_bootcount;
}

void BootloopGuard::begin() {
  s_start_time = millis();
  s_validated = false;

  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  int bank = getBankIndex(running);

  // If partition layout is not standard OTA, ensure rollback is cancelled and
  // exit
  if (bank < 0) {
    s_validated = true;
    if (running && boot && running->address == boot->address) {
      esp_ota_mark_app_valid_cancel_rollback();
    }
    return;
  }

  // Check if the running bank is flagged in trial mode
  if (!isBankInTrial(bank)) {
    s_validated = true;
    // Bank is already validated/stable. Ensure ESP-IDF otadata matches.
    if (running && boot && running->address == boot->address) {
      esp_ota_mark_app_valid_cancel_rollback();
    }
    return;
  }

  // Check non-wiped RTC RAM boot count (incremented in onBoot())
  uint8_t count = s_rtc_bootcount;

  logger.warningf(
      "[BootloopGuard] Firmware bank %d in TRIAL mode (boot attempt %u of %u)",
      bank, count, MAX_BOOT_ATTEMPTS);

  if (count >= MAX_BOOT_ATTEMPTS) {
    logger.errorf("[BootloopGuard] Boot attempt reached limit (%u)! Firmware "
                  "bank %d softlocked. Rolling back...",
                  count, bank);

    // Clear trial flag for this failing bank so we do not retry it
    setBankTrial(bank, false);
    s_rtc_bootcount = 0;

    // 1. Attempt native ESP-IDF rollback
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    logger.errorf(
        "[BootloopGuard] esp_ota_mark_app_invalid_rollback_and_reboot returned "
        "0x%x. Performing manual bank switch...",
        err);

    // 2. Fallback: find and verify alternate partition (ESPHome safe_mode
    // approach)
    const esp_partition_t *rollback_part = findAlternateAppPartition(true);
    if (!rollback_part) {
      // If image verification failed, try finding any alternate partition as
      // last resort
      rollback_part = findAlternateAppPartition(false);
    }

    if (rollback_part) {
      logger.warningf("[BootloopGuard] Selecting alternate partition '%s' "
                      "(0x%lx) and restarting...",
                      rollback_part->label, rollback_part->address);
      esp_ota_set_boot_partition(rollback_part);
    } else {
      logger.error("[BootloopGuard] Critical: No alternate OTA partition "
                   "available to rollback to!");
    }

    delay(500);
    esp_restart();
    return;
  }
}
