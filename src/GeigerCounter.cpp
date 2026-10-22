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
        return -1;
    }
    
    // Buffer is full (WINDOW_SIZE samples, spanning ~60s)
    uint32_t totalPulses = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        totalPulses += samples[i].pulseCount;
    }

    // Find oldest and newest timestamps in circular buffer
    int oldestIdx = sampleIndex; // sampleIndex is next write index, which holds oldest sample
    int newestIdx = (sampleIndex + WINDOW_SIZE - 1) % WINDOW_SIZE;
    unsigned long startTime = samples[oldestIdx].timestamp;
    unsigned long endTime = samples[newestIdx].timestamp;
    
    unsigned long intervalSpanMs = endTime - startTime;
    if (intervalSpanMs == 0) {
        return 0; // Avoid division by zero
    }
    
    // There are (WINDOW_SIZE - 1) intervals between oldest and newest timestamps.
    // Since each sample represents pulses collected over its respective interval,
    // the total time represented by all WINDOW_SIZE samples includes the oldest sample's interval:
    // totalTimeSpanMs = intervalSpanMs * WINDOW_SIZE / (WINDOW_SIZE - 1)
    float totalTimeSpanMs = (float)intervalSpanMs * (float)WINDOW_SIZE / (float)(WINDOW_SIZE - 1);
    
    // Calculate CPM: (total pulses / time elapsed in ms) * 60,000 ms/min
    float cpm = (float)totalPulses * 60000.0f / totalTimeSpanMs;
    
    return (int)(cpm + 0.5f);
}

float GeigerCounter::getDoseRate() const {
    if (!hasValidData()) {
        return -1.0f;
    }
    int cpm = getCPM();
    if (cpm < 0) {
        return -1.0f;
    }
    return cpm * cpmToUsvFactor;
}

bool GeigerCounter::hasValidData() const {
    return bufferFull;
}

bool GeigerCounter::checkAndLogHighRadiation() {
    if (!hasValidData()) {
        return false;
    }
    int cpm = getCPM();
    if (cpm > 3500) {
        logger.warningf("ALERT: Very high radiation level detected: %d CPM (>20 uSv/h)", cpm);
        return true;
    }
    return false;
}