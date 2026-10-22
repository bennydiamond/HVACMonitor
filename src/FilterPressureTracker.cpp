#include "FilterPressureTracker.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <Preferences.h>
#include <cmath>

FilterPressureTracker::FilterPressureTracker() :
    _state(STATE_IDLE),
    _filterPressure(0.0f),
    _lastSavedFilterPressure(0.0f),
    _blowerStartTimeMs(0),
    _lastSaveTimeMs(0)
{
}

void FilterPressureTracker::begin() {
    Preferences prefs;
    if (prefs.begin("hvac-config", true)) { // Read-only mode
        _filterPressure = prefs.getFloat("filter_p", 0.0f);
        _lastSavedFilterPressure = _filterPressure;
        prefs.end();
        logger.infof("FilterTracker: Loaded saved filter pressure from NVS: %.1f Pa", _filterPressure);
    } else {
        logger.warning("FilterTracker: Could not open NVS for reading filter_p, defaulting to 0.0 Pa");
        _filterPressure = 0.0f;
        _lastSavedFilterPressure = 0.0f;
    }
    _lastSaveTimeMs = millis();
}

const char* FilterPressureTracker::getStateStr() const {
    switch (_state) {
        case STATE_IDLE:     return "IDLE";
        case STATE_SPOOLING: return "SPOOLING";
        case STATE_STEADY:   return "STEADY";
        default:             return "UNKNOWN";
    }
}

void FilterPressureTracker::update(float diff_pressure_pa, float fan_amps, FanStatus fan_status, unsigned long currentTimeMs) {
    float fanOffThreshold = 0.2f;
    {
        ConfigManagerAccessor config;
        fanOffThreshold = config->getFanOffCurrentThreshold();
    }
    
    // Determine whether blower fan is electrically running
    bool blowerRunning = (fan_status != FAN_STATUS_OFF) || (fan_amps > fanOffThreshold);

    switch (_state) {
        case STATE_IDLE: {
            if (blowerRunning) {
                // Blower just energized! Transition to SPOOLING to buffer sensor lag & motor acceleration
                _state = STATE_SPOOLING;
                _blowerStartTimeMs = currentTimeMs;
                logger.infof("FilterTracker: Blower started (fan=%.2fA). Spooling (dwell 30s)...", fan_amps);
            }
            break;
        }

        case STATE_SPOOLING: {
            if (!blowerRunning) {
                // Blower shut off before dwell completed (short blast or glitch)
                logger.warningf("FilterTracker: Blower stopped early (<30s). Reverting to IDLE.");
                _state = STATE_IDLE;
            } else if (currentTimeMs - _blowerStartTimeMs >= STABILIZATION_DWELL_MS) {
                // Dwell time complete! Blower is at full operating speed and air flow is steady
                _state = STATE_STEADY;
                logger.infof("FilterTracker: Reached STEADY state. Initial stable diff P = %.1f Pa", diff_pressure_pa);
                
                // If never initialized or large step change (e.g. filter replaced), adopt value directly
                if (_filterPressure <= 0.0f || fabs(diff_pressure_pa - _filterPressure) > 25.0f) {
                    _filterPressure = diff_pressure_pa;
                } else {
                    // Blend smoothly with previous recorded run
                    _filterPressure = (_filterPressure * 0.8f) + (diff_pressure_pa * 0.2f);
                }
            }
            break;
        }

        case STATE_STEADY: {
            if (!blowerRunning) {
                // Blower shut off! Immediately lock value before airflow deceleration drags pressure down
                _state = STATE_IDLE;
                logger.infof("FilterTracker: Blower stopped. Filter pressure locked at %.1f Pa", _filterPressure);
                saveToNvs(false);
            } else {
                // Blower running steadily: smooth operating point using EMA (alpha = 0.05)
                if (diff_pressure_pa > 0.0f) {
                    _filterPressure = (_filterPressure * 0.95f) + (diff_pressure_pa * 0.05f);
                }
            }
            break;
        }
    }

    // Periodic 24-hour save check (even if blower remains idle for a whole day)
    if (currentTimeMs - _lastSaveTimeMs >= MAX_SAVE_INTERVAL_MS) {
        if (fabs(_filterPressure - _lastSavedFilterPressure) >= 0.1f) {
            saveToNvs(false);
        } else {
            _lastSaveTimeMs = currentTimeMs; // Reset timer since value did not change
        }
    }
}

void FilterPressureTracker::saveToNvs(bool force) {
    if (_filterPressure <= 0.0f) return; // Don't persist uninitialized value

    unsigned long currentTime = millis();
    float delta = fabs(_filterPressure - _lastSavedFilterPressure);
    bool shouldSave = false;

    if (force) {
        if (delta >= 0.1f) {
            shouldSave = true;
            logger.infof("FilterTracker: Force save to NVS before reboot/OTA (P=%.1f Pa)", _filterPressure);
        }
    } else {
        bool timeForDelta = (currentTime - _lastSaveTimeMs >= MIN_SAVE_INTERVAL_MS);
        bool deltaExceeded = (delta >= DELTA_SAVE_THRESHOLD_PA);
        bool dailyInterval = (currentTime - _lastSaveTimeMs >= MAX_SAVE_INTERVAL_MS);

        if ((timeForDelta && deltaExceeded) || (dailyInterval && delta >= 0.1f)) {
            shouldSave = true;
            logger.infof("FilterTracker: Scheduled save to NVS (P=%.1f Pa, delta=%.1f Pa, elapsed=%lu s)", 
                         _filterPressure, delta, (currentTime - _lastSaveTimeMs) / 1000);
        }
    }

    if (shouldSave) {
        Preferences prefs;
        if (prefs.begin("hvac-config", false)) { // Read-write mode
            if (prefs.putFloat("filter_p", _filterPressure) > 0) {
                _lastSavedFilterPressure = _filterPressure;
                _lastSaveTimeMs = currentTime;
                logger.infof("FilterTracker: Successfully saved filter_p=%.1f to NVS", _filterPressure);
            } else {
                logger.error("FilterTracker: Failed to write filter_p to NVS");
            }
            prefs.end();
        } else {
            logger.error("FilterTracker: Failed to open NVS namespace hvac-config");
        }
    }
}

void FilterPressureTracker::flush() {
    saveToNvs(true);
}
