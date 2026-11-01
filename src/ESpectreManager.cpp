#include "ESpectreManager.h"
#include "SensorDataAccessor.h"
#include "DiagnosticDataAccessor.h"
#include "Logger.h"

#include <WiFi.h>
#include <cstring>
#include <cmath>
#include <cstdio>

extern Logger logger;

namespace {

AppLogLevel to_app_log_level(espectre::LogLevel level) {
  switch (level) {
    case espectre::LogLevel::ERROR:
      return APP_LOG_ERROR;
    case espectre::LogLevel::WARNING:
      return APP_LOG_WARNING;
    case espectre::LogLevel::INFO:
      return APP_LOG_INFO;
    case espectre::LogLevel::DEBUG:
    case espectre::LogLevel::VERBOSE:
      return APP_LOG_DEBUG;
  }
  return APP_LOG_INFO;
}

bool espectre_log_enabled(void *, espectre::LogLevel level, const char *tag) {
  return tag != nullptr && to_app_log_level(level) >= logger.getLogLevel();
}

void espectre_log_write(void *, espectre::LogLevel level, const char *tag, int,
                        const char *format, va_list args) {
  char buf[256];
  int written = snprintf(buf, sizeof(buf), "[%s] ", tag ? tag : "espectre");
  if (written > 0 && static_cast<size_t>(written) < sizeof(buf)) {
    vsnprintf(buf + written, sizeof(buf) - static_cast<size_t>(written), format, args);
  } else {
    vsnprintf(buf, sizeof(buf), format, args);
  }
  logger.log(to_app_log_level(level), buf);
}

}  // namespace

ESpectreManager& ESpectreManager::getInstance() {
    static ESpectreManager instance;
    return instance;
}

ESpectreManager::ESpectreManager() {
    _runtime.config().device_id = espectre::derive_runtime_device_id();
    _runtime.config().runtime_detector_selection_enabled = true;
    _runtime.config().persist_runtime_overrides = true;
    _runtime.config().wifi_scan_results_managed_externally = true;
}

ESpectreManager::~ESpectreManager() {
    _directBridge.shutdown();
    _runtime.shutdown();
    espectre::clear_log_sink();
}

void ESpectreManager::init() {
    if (_initialized) return;

    espectre::set_log_sink({nullptr, &espectre_log_enabled, &espectre_log_write});

    if (!_runtime.setup(this)) {
        logger.error("Failed to initialize ESPectre runtime");
        return;
    }

    espectre::RuntimeDirectHttpBridgeConfig bridge_config{};
    bridge_config.frontend = "native";
    bridge_config.device_name = "HVACMonitor";
    bridge_config.hostname = "hvacmonitor";
    bridge_config.firmware_version = "1.0.0";
    bridge_config.chip = CONFIG_IDF_TARGET;
    bridge_config.device_id = _runtime.config().device_id;
    bridge_config.port = espectre::ESPECTRE_DIRECT_HTTP_PORT;
    bridge_config.raw_csi = false;
    bridge_config.allow_missing_origin = true;
    bridge_config.device_label_getter = []() { return std::string("HVACMonitor"); };
    bridge_config.diagnostics_sample_getter = [this]() { return _runtime.diagnostics_sample(); };
    bridge_config.loop_time_ms_getter = [this]() { return _lastLoopTimeMs; };
    bridge_config.runtime_events = &_runtimeEvents;
    bridge_config.wifi_snapshot_getter = []() {
        espectre::DirectWifiSnapshot snap{};
        String ssid = WiFi.SSID();
        snap.configured = (ssid.length() > 0);
        snap.connected = (WiFi.status() == WL_CONNECTED);
        snap.ssid = ssid.c_str();
        if (snap.connected) {
            snap.bssid = WiFi.BSSIDstr().c_str();
            snap.channel = WiFi.channel();
            snap.band = snap.channel > 14 ? "5g" : "2g";
            snap.rssi_dbm = static_cast<int16_t>(WiFi.RSSI());
        }
        return snap;
    };

    if (!_directBridge.setup(&_directService, &_runtime, bridge_config, [this]() {
        updateSensorAccessor(_runtime.snapshot());
        updateDiagnosticAccessor();
    })) {
        logger.errorf("Failed to setup ESPectre Direct HTTP Bridge on port %u", bridge_config.port);
    } else {
        logger.infof("ESPectre Direct HTTP listening on port %u", bridge_config.port);
    }

    _initialized = true;
    logger.info("ESPectre Wi-Fi CSI sensing engine initialized");
    updateSensorAccessor(_runtime.snapshot());
    updateDiagnosticAccessor();
}

void ESpectreManager::loop() {
    if (!_initialized) return;

    uint32_t startMs = millis();

    applyPendingCommands();

    _runtime.loop();
    drainPendingRuntimeEvents();
    _directBridge.loop();

    const bool liveClients = _directBridge.event_client_count() > 0;
    if (liveClients != _liveTelemetryEnabled) {
        _liveTelemetryEnabled = liveClients;
        _runtime.set_live_telemetry_enabled(liveClients);
    }

    uint32_t now = millis();
    _lastLoopTimeMs = static_cast<float>(now - startMs);

    if (now - _lastDiagnosticUpdateMs >= 1000) {
        _lastDiagnosticUpdateMs = now;
        updateDiagnosticAccessor();
    }
}

void ESpectreManager::drainPendingRuntimeEvents() {
    espectre::RuntimeSnapshot snapshot;
    while (_runtimeEvents.take_motion_state(snapshot)) {
        updateSensorAccessor(snapshot);
        _directBridge.publish_changes(espectre::FrontendCommandChange::SENSING);
    }
    if (_runtimeEvents.take_live_telemetry(snapshot)) {
        updateSensorAccessor(snapshot);
        if (_liveTelemetryEnabled) {
            _directBridge.publish_motion(snapshot);
        }
    }
    float threshold = 0.0f;
    if (_runtimeEvents.take_threshold(threshold)) {
        _directBridge.publish_changes(espectre::FrontendCommandChange::SENSING);
    }
}

void ESpectreManager::applyPendingCommands() {
    std::lock_guard<std::mutex> lock(_cmdMutex);
    bool changed = false;

    if (_hasPendingArmed) {
        _runtime.set_services_armed(_pendingArmed);
        _hasPendingArmed = false;
        changed = true;
        logger.infof("ESPectre sensing services %s", _pendingArmed ? "armed" : "disarmed");
    }

    if (_hasPendingProfile) {
        if (_runtime.set_detection_algorithm(_pendingProfile)) {
            changed = true;
            logger.infof("ESPectre detection profile changed to: %s",
                         espectre::detection_algorithm_name(_pendingProfile));
        } else {
            logger.warning("Failed to switch ESPectre detection profile");
        }
        _hasPendingProfile = false;
    }

    if (_hasPendingTrafficMode) {
        if (_runtime.set_traffic_generator_mode(_pendingTrafficMode)) {
            changed = true;
            logger.infof("ESPectre CSI traffic mode changed to: %s",
                         espectre::traffic_generator_mode_name(_pendingTrafficMode));
        } else {
            logger.warning("Failed to switch ESPectre CSI traffic mode");
        }
        _hasPendingTrafficMode = false;
    }

    if (_hasPendingHits) {
        if (_runtime.set_motion_hits(_pendingOnHits, _pendingOffHits)) {
            changed = true;
            logger.infof("ESPectre motion hits set to: on=%u, off=%u", _pendingOnHits, _pendingOffHits);
        } else {
            logger.warning("Failed to set ESPectre motion hits");
        }
        _hasPendingHits = false;
    }

    if (_hasPendingThreshold) {
        if (_runtime.set_threshold(_pendingThreshold)) {
            changed = true;
            logger.infof("ESPectre threshold set to: %.4f", _pendingThreshold);
        } else {
            logger.warningf("Failed to set ESPectre threshold: %.4f", _pendingThreshold);
        }
        _hasPendingThreshold = false;
    }

    if (_hasPendingRecalibrate) {
        if (_runtime.trigger_recalibration()) {
            changed = true;
            logger.info("ESPectre baseline recalibration triggered");
        } else {
            logger.warning("Failed to trigger ESPectre recalibration");
        }
        _hasPendingRecalibrate = false;
    }

    if (changed) {
        _directBridge.publish_changes(espectre::FrontendCommandChange::SENSING);
    }
}

bool ESpectreManager::setSensingEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(_cmdMutex);
    _pendingArmed = enabled;
    _hasPendingArmed = true;
    return true;
}

bool ESpectreManager::setThreshold(float threshold) {
    if (threshold < 0.0f || threshold > 1.0f) return false;
    std::lock_guard<std::mutex> lock(_cmdMutex);
    _pendingThreshold = threshold;
    _hasPendingThreshold = true;
    return true;
}

bool ESpectreManager::setMotionHits(uint8_t on_hits, uint8_t off_hits) {
    if (on_hits < 1 || on_hits > 20 || off_hits < 1 || off_hits > 20) return false;
    std::lock_guard<std::mutex> lock(_cmdMutex);
    _pendingOnHits = on_hits;
    _pendingOffHits = off_hits;
    _hasPendingHits = true;
    return true;
}

bool ESpectreManager::setDetectionProfile(const char* profile) {
    if (!profile) return false;
    espectre::DetectionAlgorithm algo = espectre::parse_detection_algorithm(profile);
    std::lock_guard<std::mutex> lock(_cmdMutex);
    _pendingProfile = algo;
    _hasPendingProfile = true;
    return true;
}

bool ESpectreManager::setTrafficMode(const char* mode) {
    if (!mode) return false;
    espectre::TrafficGeneratorMode tMode = espectre::parse_traffic_generator_mode(mode);
    std::lock_guard<std::mutex> lock(_cmdMutex);
    _pendingTrafficMode = tMode;
    _hasPendingTrafficMode = true;
    return true;
}

bool ESpectreManager::triggerRecalibration() {
    std::lock_guard<std::mutex> lock(_cmdMutex);
    _hasPendingRecalibrate = true;
    return true;
}

bool ESpectreManager::isSensingEnabled() const {
    return _runtime.services_armed();
}

float ESpectreManager::getThreshold() const {
    return _runtime.snapshot().threshold;
}

uint8_t ESpectreManager::getMotionOnHits() const {
    return _runtime.config().motion_on_hits;
}

uint8_t ESpectreManager::getMotionOffHits() const {
    return _runtime.config().motion_off_hits;
}

const char* ESpectreManager::getDetectionProfile() const {
    return espectre::detection_algorithm_name(_runtime.config().detection_algorithm);
}

const char* ESpectreManager::getTrafficMode() const {
    return espectre::traffic_generator_mode_name(_runtime.config().traffic_generator_mode);
}

bool ESpectreManager::isCalibrating() const {
    return _runtime.snapshot().calibrating;
}

bool ESpectreManager::isReady() const {
    return _runtime.snapshot().ready_to_publish;
}

bool ESpectreManager::isMotionDetected() const {
    return _runtime.snapshot().motion_state == espectre::MotionState::MOTION;
}

float ESpectreManager::getMotionIntensity() const {
    return _runtime.snapshot().movement_metric;
}

void ESpectreManager::updateSensorAccessor(const espectre::RuntimeSnapshot &snapshot) {
    SensorDataAccessor sensor;
    sensor->motion_detected = (snapshot.motion_state == espectre::MotionState::MOTION);
    sensor->motion_intensity = snapshot.movement_metric;
    sensor->motion_ready = snapshot.ready_to_publish;
    sensor->motion_calibrating = snapshot.calibrating;
}

void ESpectreManager::updateDiagnosticAccessor() {
    const espectre::RuntimeDiagnosticsSample *sample = _runtime.diagnostics_sample();
    const espectre::RuntimeSnapshot &snapshot = _runtime.snapshot();

    DiagnosticDataAccessor diag;
    diag->motion.enabled = _runtime.services_armed();
    diag->motion.calibrating = snapshot.calibrating;
    diag->motion.ready = snapshot.ready_to_publish;
    diag->motion.motionDetected = (snapshot.motion_state == espectre::MotionState::MOTION);
    diag->motion.intensity = snapshot.movement_metric;
    diag->motion.threshold = snapshot.threshold;
    snprintf(diag->motion.profile, sizeof(diag->motion.profile), "%s",
             espectre::detection_algorithm_name(_runtime.config().detection_algorithm));
    snprintf(diag->motion.trafficMode, sizeof(diag->motion.trafficMode), "%s",
             espectre::traffic_generator_mode_name(_runtime.config().traffic_generator_mode));
    diag->motion.motionOnHits = _runtime.config().motion_on_hits;
    diag->motion.motionOffHits = _runtime.config().motion_off_hits;

    if (sample != nullptr) {
        diag->motion.csiCallbackRate = sample->csi_callback_pps;
        diag->motion.csiAcceptedRate = sample->csi_accepted_pps;
        diag->motion.csiAdmittedRate = sample->csi_admitted_pps;
        diag->motion.csiFilteredRate = sample->csi_filtered_pps;
        diag->motion.csiMissingRate = sample->csi_missing_slots_pps;
        diag->motion.csiExcessRate = sample->csi_excess_pps;
        diag->motion.csiStaleRate = sample->csi_stale_pps;
        diag->motion.csiOutOfOrderRate = sample->csi_out_of_order_pps;
        diag->motion.csiOccupancy = sample->csi_occupancy_ratio;
        diag->motion.generatorRate = sample->generator_pps;
        diag->motion.wifiRssi = sample->wifi_rssi_dbm;
        diag->motion.wifiChannel = sample->wifi_channel;
    }
}

void ESpectreManager::on_sensing_readiness_changed(const espectre::RuntimeSnapshot &snapshot) {
    updateSensorAccessor(snapshot);
    _directBridge.publish_changes(espectre::FrontendCommandChange::SENSING);
    logger.infof("ESPectre readiness changed: ready=%d, calibrating=%d",
                 snapshot.ready_to_publish, snapshot.calibrating);
}

void ESpectreManager::on_motion_state_changed(const espectre::RuntimeSnapshot &snapshot) {
    if (!snapshot.ready_to_publish) return;
    _runtimeEvents.post_motion_state(snapshot);
    logger.infof("ESPectre motion state changed: %s (metric=%.4f, threshold=%.4f)",
                 snapshot.motion_state == espectre::MotionState::MOTION ? "MOTION" : "IDLE",
                 snapshot.movement_metric, snapshot.threshold);
}

void ESpectreManager::on_periodic_update(const espectre::RuntimeSnapshot &snapshot, uint32_t csi_accepted) {
    (void)csi_accepted;
    updateSensorAccessor(snapshot);
}

void ESpectreManager::on_threshold_changed(const espectre::RuntimeSnapshot &snapshot) {
    _runtimeEvents.post_threshold(snapshot.threshold);
}

void ESpectreManager::on_detector_changed(const espectre::RuntimeSnapshot &snapshot) {
    updateSensorAccessor(snapshot);
    _directBridge.publish_changes(espectre::FrontendCommandChange::SENSING);
    logger.infof("ESPectre detector changed: %s", snapshot.detector_name);
}

void ESpectreManager::on_calibration_started(const espectre::RuntimeSnapshot &snapshot) {
    updateSensorAccessor(snapshot);
    _directBridge.publish_changes(espectre::FrontendCommandChange::SENSING);
    logger.info("ESPectre threshold calibration started");
}

void ESpectreManager::on_calibration_finished(const espectre::RuntimeSnapshot &snapshot, bool success) {
    (void)success;
    updateSensorAccessor(snapshot);
    _directBridge.publish_changes(espectre::FrontendCommandChange::SENSING);
    logger.infof("ESPectre threshold calibration %s (threshold=%.4f)",
                 success ? "succeeded" : "failed/aborted", snapshot.threshold);
}

void ESpectreManager::on_live_telemetry(const espectre::RuntimeSnapshot &snapshot) {
    if (!snapshot.ready_to_publish) return;
    _runtimeEvents.post_live_telemetry(snapshot);
}

void ESpectreManager::on_runtime_fault(const char *message) {
    logger.errorf("ESPectre runtime fault: %s", message ? message : "unknown");
    espectre::EspectreDeviceConfig dev;
    dev.device_id = _runtime.config().device_id;
    _directBridge.publish_event("fault", espectre::espectre_fault_payload(dev, message ? message : "", millis()));
}

