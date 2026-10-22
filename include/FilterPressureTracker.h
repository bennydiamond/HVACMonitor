#pragma once

#include <Arduino.h>
#include "ui/ui_common.h"

class FilterPressureTracker {
public:
    FilterPressureTracker();
    ~FilterPressureTracker() = default;

    void begin();
    
    // Updates tracker with latest readings from the sensor stack
    void update(float diff_pressure_pa, float fan_amps, FanStatus fan_status, unsigned long currentTimeMs);

    // Returns the stabilized / recorded filter differential pressure (in Pa)
    float getFilterPressure() const { return _filterPressure; }

    // Returns whether the blower is currently in steady-state operation
    bool isSteadyState() const { return _state == STATE_STEADY; }

    // Returns the current tracker state string for diagnostics
    const char* getStateStr() const;

    // Forces a flush of the current filter pressure to NVS if changed
    void flush();

private:
    enum BlowerState {
        STATE_IDLE,       // Blower fan is OFF (~0 Pa differential pressure)
        STATE_SPOOLING,   // Blower turned ON, waiting 30s stabilization dwell time
        STATE_STEADY      // Blower running steadily, differential pressure active & tracking
    };

    void saveToNvs(bool force = false);

    BlowerState _state;
    float _filterPressure;              // Active/latched clogged filter pressure (Pa)
    float _lastSavedFilterPressure;     // Value currently stored in NVS flash
    
    unsigned long _blowerStartTimeMs;   // Timestamp when blower turned ON
    unsigned long _lastSaveTimeMs;      // Timestamp of last NVS write
    
    static constexpr unsigned long STABILIZATION_DWELL_MS = 30000;       // 30 seconds
    static constexpr unsigned long MIN_SAVE_INTERVAL_MS = 2 * 3600000UL; // 2 hours
    static constexpr unsigned long MAX_SAVE_INTERVAL_MS = 24 * 3600000UL;// 24 hours
    static constexpr float DELTA_SAVE_THRESHOLD_PA = 3.0f;               // 3 Pa swing threshold
};
