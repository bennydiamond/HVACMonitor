#pragma once

#include <Arduino.h>

class GeigerCounter {
public:
    // Fixed window size to avoid dynamic memory allocation
    static const uint8_t WINDOW_SIZE = 30;
    
    GeigerCounter();

    // Add a new pulse count sample
    void addSample(uint16_t pulseCount);
    
    // Get the calculated CPM
    int getCPM() const;
    
    // Get the calculated radiation dose in µSv/h
    float getDoseRate() const;
    
    // Check if radiation level is high and log warning if needed
    // Returns true if radiation level is high
    bool checkAndLogHighRadiation();
    
    // Check if we have enough data for calculations
    bool hasValidData() const;

private:
    struct GeigerSample {
        uint16_t pulseCount;
        unsigned long timestamp;
    };

    GeigerSample samples[WINDOW_SIZE];
    uint8_t sampleIndex;
    bool bufferFull;
    
    // Conversion factor from CPM to µSv/h (depends on tube sensitivity)
    // For J305 beta/gamma tube used with Cajoe 1.1 radiation detector board
    // J305 tube produces ~153 CPM at 1 uSv/h exposure
    // This gives ~0.00654 uSv/h per CPM
    float cpmToUsvFactor = 1.0f / 153.0f;
};