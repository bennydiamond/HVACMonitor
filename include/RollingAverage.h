#ifndef ROLLING_AVERAGE_H
#define ROLLING_AVERAGE_H

#include <Arduino.h>
#include <deque>

class RollingAverage {
public:
    /**
     * @brief Construct a new Rolling Average object.
     * @param windowMs The time window in milliseconds to average over.
     * @param maxSamples A hard limit on the number of samples to store to prevent memory exhaustion.
     */
    RollingAverage(unsigned long windowMs, size_t maxSamples);

    /**
     * @brief Adds a new value to the data set.
     * It removes values older than the time window and also removes the oldest values if the sample limit is exceeded.
     */
    void add(float newValue);

    /**
     * @brief Calculates and returns the current average of the values within the time window.
     * @return The calculated rolling average.
     */
    float getAverage();

private:
    // A structure to hold a sensor value and its timestamp.
    struct DataPoint {
        unsigned long timestamp;
        float value;
    };

    std::deque<DataPoint> history;  // Stores the recent data points.
    unsigned long windowMs;         // The time window in milliseconds.
    size_t maxSamples;              // The maximum number of samples to store.
    float lastValue;                // Caches the most recent value.
};

#endif // ROLLING_AVERAGE_H