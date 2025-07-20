#include "GeigerCounter.h"
#include "Logger.h"

GeigerCounter::GeigerCounter() : 
    sampleIndex(0),
    bufferFull(false) {
    // Initialize all samples to zero
    for (uint8_t i = 0; i < WINDOW_SIZE; i++) {
        samples[i] = {0, 0};
    }
}

void GeigerCounter::addSample(uint16_t pulseCount) {
    samples[sampleIndex].pulseCount = pulseCount;
    samples[sampleIndex].timestamp = millis();
    sampleIndex = (sampleIndex + 1) % WINDOW_SIZE;
    if (sampleIndex == 0) {
        bufferFull = true;
    }
}

int GeigerCounter::getCPM() const {
    if (!hasValidData()) {
        return 0;
    }
    
    // Calculate total pulses and time span
    uint32_t totalPulses = 0;
    unsigned long startTime, endTime;
    
    if (bufferFull) {
        // Buffer is full, use all samples
        for (int i = 0; i < WINDOW_SIZE; i++) {
            totalPulses += samples[i].pulseCount;
        }
        // Find oldest and newest timestamps
        int oldestIdx = sampleIndex;
        int newestIdx = (sampleIndex + WINDOW_SIZE - 1) % WINDOW_SIZE;
        startTime = samples[oldestIdx].timestamp;
        endTime = samples[newestIdx].timestamp;
    } else {
        // Buffer is partially filled
        for (int i = 0; i < sampleIndex; i++) {
            totalPulses += samples[i].pulseCount;
        }
        startTime = samples[0].timestamp;
        endTime = samples[sampleIndex - 1].timestamp;
    }
    
    unsigned long timeSpanMs = endTime - startTime;
    if (timeSpanMs == 0) {
        return 0; // Avoid division by zero
    }
    
    // Calculate CPM: (total pulses / time elapsed in ms) * 60,000 ms/min
    float cpm = (float)totalPulses * 60000.0f / timeSpanMs;
    
    return (int)cpm;
}

float GeigerCounter::getDoseRate() const {
    int cpm = getCPM();
    return cpm * cpmToUsvFactor;
}

bool GeigerCounter::hasValidData() const {
    return bufferFull || (sampleIndex > 0);
}

bool GeigerCounter::checkAndLogHighRadiation() {
    int cpm = getCPM();
    if (cpm > 3500) {
        logger.warningf("ALERT: Very high radiation level detected: %d CPM (>20 uSv/h)", cpm);
        return true;
    }
    return false;
}