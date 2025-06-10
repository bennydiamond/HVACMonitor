#include "RollingAverage.h"

// Constructor implementation
RollingAverage::RollingAverage(unsigned long windowMs, size_t maxSamples)
    : windowMs(windowMs), maxSamples(maxSamples), lastValue(0.0f) {
    // The initializer list is used to set the window, sample limit, and lastValue.
}

// add() method implementation
void RollingAverage::add(float newValue) {
    unsigned long currentTime = millis();
    this->lastValue = newValue; // Cache the latest value

    // Add the new data point to the back of the deque.
    history.push_back({currentTime, newValue});

    // 1. Remove old data points that are outside the time window.
    unsigned long cutoffTime = currentTime - this->windowMs;
    while (!history.empty() && history.front().timestamp < cutoffTime) {
        history.pop_front();
    }

    // 2. Enforce the memory safety cap. If we're still over the sample limit
    // (due to a high frequency of data), remove the oldest elements until we're within the limit.
    while (history.size() > this->maxSamples) {
        history.pop_front();
    }
}

// getAverage() method implementation
float RollingAverage::getAverage() {
    // If the history is empty, return the last known value to avoid division by zero.
    if (history.empty()) {
        return this->lastValue;
    }

    // Sum all the values currently in the history deque.
    float sum = 0.0f;
    for (const auto& point : history) {
        sum += point.value;
    }

    // Return the calculated average.
    return sum / history.size();
}