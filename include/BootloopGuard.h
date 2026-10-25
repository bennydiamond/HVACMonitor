#pragma once

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

/**
 * @brief BootloopGuard provides an automated firmware stability verification
 * system
 *
 * When an OTA update is flashed:
 * 1. The target partition's trial bit in NVS is set to 1.
 * 2. The RTC RAM boot counter (stored in non-wiped memory using a 16-bit
 * nibble-inverted checksum) is reset to 0.
 * 3. On each boot while the active bank is in trial mode, the boot counter
 * increments.
 * 4. If the device runs stably for 60 seconds (1 minute), the firmware is
 * deemed valid, the NVS trial flag is cleared, the counter is reset, and
 *    esp_ota_mark_app_valid_cancel_rollback() is called.
 * 5. If the device reboots repeatedly and the boot counter reaches 10, the
 * failing bank is invalidated, the trial flag is dropped, the alternate
 * firmware bank is selected, and the device reboots to the rollback bank.
 * 6. Flashing via SD card clears all trial flags and resets the boot counter.
 */
class BootloopGuard {
public:
  // Register boot attempt immediately on entry to setup() before SD check or peripherals.
  static void onBoot();

  // Initialize and evaluate bootloop guard threshold right after SD update check.
  static void begin();

  // Periodic check called from main loop() to verify when the 60s stability
  // window has elapsed.
  static void handle();

  // Mark that an OTA update was completed and the target partition is pending
  // trial validation.
  static void markOtaTrialPending();

  // Clear all pending trial flags and reset the boot count (called on SD card
  // firmware flash).
  static void onSdUpdate();

  // Manually mark the current firmware partition as valid and cancel rollback.
  static void markFirmwareValid();

  // Cancel ESP-IDF bootloader rollback immediately upon completing the first main loop cycle.
  static void cancelBootloaderRollback();

  // Override Arduino weak hook to prevent premature auto-validation of OTA image on boot.
  // Delaying validation allows BootloopGuard to enforce the stability window.
  static bool verifyRollbackLater();

  // Called on orderly reboot to prevent intentional restarts from incrementing
  // crash count
  static void onGracefulShutdown();

  // Check if the currently running firmware partition is in trial mode.
  static bool isRunningInTrial();

  // Get the current boot count during trial (1 to 10), or 0 if not in trial.
  static uint8_t getBootCount();

  // Packed 16-bit storage representing redundant nibbles with bitwise inversion checks
  struct __attribute__((packed)) BootCountStorage {
    uint8_t inv_low  : 4; // bits 0..3:  ~count
    uint8_t count_low: 4; // bits 4..7:   count
    uint8_t inv_high : 4; // bits 8..11: ~count
    uint8_t count_high: 4;// bits 12..15: count

    inline bool isValid() const {
      return (count_low == count_high) &&
             ((count_low ^ inv_low) == 0x0F) &&
             ((count_high ^ inv_high) == 0x0F);
    }

    // Assign integer value (encodes redundant inverted nibbles)
    inline BootCountStorage& operator=(uint8_t count) {
      uint8_t c = count & 0x0F;
      uint8_t inv = (~c) & 0x0F;
      count_low = c;
      count_high = c;
      inv_low = inv;
      inv_high = inv;
      return *this;
    }

    // Implicit conversion to uint8_t (validates checksum, returns 0 if corrupted/uninitialized)
    inline operator uint8_t() const {
      return isValid() ? count_low : 0;
    }

    // Prefix increment (++s_rtc_bootcount)
    inline BootCountStorage& operator++() {
      uint8_t current = isValid() ? count_low : 0;
      *this = static_cast<uint8_t>(current + 1);
      return *this;
    }

    // Postfix increment (s_rtc_bootcount++)
    inline uint8_t operator++(int) {
      uint8_t current = isValid() ? count_low : 0;
      *this = static_cast<uint8_t>(current + 1);
      return current;
    }
  };

private:
  static uint32_t s_start_time;
  static bool s_validated;

  static uint8_t readNvsTrialFlags();
  static void writeNvsTrialFlags(uint8_t flags);
  static int getBankIndex(const esp_partition_t *partition);
  static bool isBankInTrial(int bank);
  static void setBankTrial(int bank, bool inTrial);
};
