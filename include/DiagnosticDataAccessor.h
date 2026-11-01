#pragma once

#include "BaseAccessor.h"
#include <cstdint>

struct SPS30QueryData {
    bool valid = false;
    uint32_t timestamp = 0; // millis() when response was received
    uint8_t fwMajor = 0;
    uint8_t fwMinor = 0;
    uint32_t fanInterval = 0;
    uint32_t fanDays = 0;
    uint32_t statusReg = 0;
};

struct SGP41QueryData {
    bool valid = false;
    uint32_t timestamp = 0; // millis() when response was received
    int retStatus = -1;
    uint16_t rawValue = 0;
};

struct SCD30QueryData {
    bool valid = false;
    uint32_t timestamp = 0; // millis() when response was received
    bool autoCalState = false;
    uint16_t forceCalValue = 0;
};

struct MotionDiagnosticData {
    bool enabled = true;
    bool calibrating = false;
    bool ready = false;
    bool motionDetected = false;
    float intensity = 0.0f;
    float threshold = 0.0f;
    char profile[24] = "lightweight";
    char trafficMode[24] = "ping";
    uint8_t motionOnHits = 4;
    uint8_t motionOffHits = 3;

    // Rates (pps) & link diagnostics
    float csiCallbackRate = 0.0f;
    float csiAcceptedRate = 0.0f;
    float csiAdmittedRate = 0.0f;
    float csiFilteredRate = 0.0f;
    float csiMissingRate = 0.0f;
    float csiExcessRate = 0.0f;
    float csiStaleRate = 0.0f;
    float csiOutOfOrderRate = 0.0f;
    float csiOccupancy = 0.0f;
    float generatorRate = 0.0f;
    int8_t wifiRssi = 0;
    uint8_t wifiChannel = 0;
};

struct DiagnosticData {
    SPS30QueryData sps30;
    SGP41QueryData sgp41;
    SCD30QueryData scd30;
    MotionDiagnosticData motion;
};

/**
 * @brief Thread-safe accessor inheriting from BaseAccessor, providing centralized
 *        mutex-protected read/write access to manual query and diagnostic snapshot data.
 */
class DiagnosticDataAccessor : public BaseAccessor {
public:
    DiagnosticDataAccessor();
    ~DiagnosticDataAccessor() override = default;

    DiagnosticData* operator->() { return &_state; }
    DiagnosticData& operator*() { return _state; }
    const DiagnosticData* operator->() const { return &_state; }
    const DiagnosticData& operator*() const { return _state; }

    static void init();

private:
    static SemaphoreHandle_t _sharedMutex;
    static StaticSemaphore_t _mutexBuffer;
    static DiagnosticData _state;
};

