#pragma once

#include "espectre_services_sdk.h"
#include <mutex>
#include <cstdint>

class ESpectreManager : public espectre::IRuntimeListener {
public:
    static ESpectreManager& getInstance();

    void init();
    void loop();

    // Control methods (thread-safe, can be called from web server / MQTT / UI)
    bool setSensingEnabled(bool enabled);
    bool setThreshold(float threshold);
    bool setMotionHits(uint8_t on_hits, uint8_t off_hits);
    bool setDetectionProfile(const char* profile);
    bool setTrafficMode(const char* mode);
    bool triggerRecalibration();

    // Getters for current effective state & config
    bool isSensingEnabled() const;
    float getThreshold() const;
    uint8_t getMotionOnHits() const;
    uint8_t getMotionOffHits() const;
    const char* getDetectionProfile() const;
    const char* getTrafficMode() const;
    bool isCalibrating() const;
    bool isReady() const;
    bool isMotionDetected() const;
    float getMotionIntensity() const;

protected:
    // IRuntimeListener callbacks (called from runtime loop on mainTask)
    void on_sensing_readiness_changed(const espectre::RuntimeSnapshot &snapshot) override;
    void on_motion_state_changed(const espectre::RuntimeSnapshot &snapshot) override;
    void on_periodic_update(const espectre::RuntimeSnapshot &snapshot, uint32_t csi_accepted) override;
    void on_threshold_changed(const espectre::RuntimeSnapshot &snapshot) override;
    void on_detector_changed(const espectre::RuntimeSnapshot &snapshot) override;
    void on_calibration_started(const espectre::RuntimeSnapshot &snapshot) override;
    void on_calibration_finished(const espectre::RuntimeSnapshot &snapshot, bool success) override;
    void on_live_telemetry(const espectre::RuntimeSnapshot &snapshot) override;
    void on_runtime_fault(const char *message) override;

private:
    ESpectreManager();
    ~ESpectreManager() override;
    ESpectreManager(const ESpectreManager&) = delete;
    ESpectreManager& operator=(const ESpectreManager&) = delete;

    void updateSensorAccessor(const espectre::RuntimeSnapshot &snapshot);
    void updateDiagnosticAccessor();
    void applyPendingCommands();
    void drainPendingRuntimeEvents();

    espectre::RuntimeFrontendController _runtime;
    espectre::RuntimeEventMailbox _runtimeEvents;
    espectre::EspIdfDirectHttpService _directService;
    espectre::RuntimeDirectHttpBridge _directBridge;
    bool _initialized = false;
    bool _liveTelemetryEnabled = false;
    float _lastLoopTimeMs = 0.0f;

    // Mutex protecting pending control commands from other tasks
    mutable std::mutex _cmdMutex;

    bool _hasPendingArmed = false;
    bool _pendingArmed = true;

    bool _hasPendingThreshold = false;
    float _pendingThreshold = 0.0f;

    bool _hasPendingHits = false;
    uint8_t _pendingOnHits = 4;
    uint8_t _pendingOffHits = 3;

    bool _hasPendingProfile = false;
    espectre::DetectionAlgorithm _pendingProfile = espectre::DetectionAlgorithm::LIGHTWEIGHT;

    bool _hasPendingTrafficMode = false;
    espectre::TrafficGeneratorMode _pendingTrafficMode = espectre::TrafficGeneratorMode::PING;

    bool _hasPendingRecalibrate = false;

    uint32_t _lastDiagnosticUpdateMs = 0;
};

