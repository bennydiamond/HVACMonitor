#ifndef ROLLING_AVERAGE_H
#define ROLLING_AVERAGE_H

#include <Arduino.h>

template<typename T = float>
class RollingAverage {
public:
    // Time window in milliseconds (30 minutes)
    static const unsigned long WINDOW_MS = 30 * 60 * 1000;

    /**
     * @brief Construct a new Rolling Average object.
     * @param num_samples The maximum number of samples to store in the history buffer.
     */
    explicit RollingAverage(size_t num_samples = 64)
        : max_samples(num_samples),
          history(new DataPoint[num_samples]),
          count(0),
          head(0),
          tail(0),
          lastValue(0)
    {
        // It's good practice to check if the allocation was successful,
        // though on ESP32 a failed 'new' will likely cause a reboot.
        if (history == nullptr) {
            // Handle memory allocation failure, e.g., by logging an error.
            // On an embedded system, this is a critical failure.
        }
    }

    /**
     * @brief Destructor to free the dynamically allocated memory.
     */
    ~RollingAverage() {
        delete[] history;
    }

    // --- Rule of Three/Five ---
    // Prevent copying to avoid double-free errors and shallow copies.
    RollingAverage(const RollingAverage&) = delete;
    RollingAverage& operator=(const RollingAverage&) = delete;


    /**
     * @brief Adds a new value to the data set.
     * It removes values older than the time window and also removes the oldest values if the sample limit is exceeded.
     * @param newValue The new value to add.
     */
    void add(T newValue) {
        // Ensure history is valid before proceeding
        if (history == nullptr) return;

        unsigned long currentTime = millis();
        this->lastValue = newValue; // Cache the latest value

        // Add the new data point
        history[head] = {currentTime, newValue};
        head = (head + 1) % max_samples;
        
        // Increase count if not full yet
        if (count < max_samples) {
            count++;
        } else {
            // Buffer is full, the oldest value is overwritten, so move tail forward
            tail = (tail + 1) % max_samples;
        }

        // Remove old data points that are outside the time window
        unsigned long cutoffTime = currentTime - WINDOW_MS;
        while (count > 0 && history[tail].timestamp < cutoffTime) {
            tail = (tail + 1) % max_samples;
            count--;
        }
    }

    /**
     * @brief Calculates and returns the current average of the values within the time window.
     * @return The calculated rolling average.
     */
    T getAverage() {
        // Ensure history is valid and not empty
        if (history == nullptr || count == 0) {
            return this->lastValue; // Return the last known value to avoid division by zero.
        }

        // Sum all the values currently in the history
        // Use double for precision regardless of T type
        double sum = 0.0;
        size_t current_pos = tail;
        for (size_t i = 0; i < count; i++) {
            sum += static_cast<double>(history[current_pos].value);
            current_pos = (current_pos + 1) % max_samples;
        }

        // Return the calculated average, cast back to T
        return static_cast<T>(sum / count);
    }

    /**
     * @brief Returns if the history buffer is empty.
     * @return True if the history buffer is empty, false otherwise.
     */
    bool isEmpty() const {
        // Check if the history buffer is empty
        return count == 0;
    }

private:
    // A structure to hold a sensor value and its timestamp.
    struct DataPoint {
        unsigned long timestamp;
        T value;
    };

    size_t max_samples;             // The maximum number of samples for this instance
    DataPoint* history;             // Pointer to the dynamically allocated history buffer
    size_t count;                   // Current number of valid samples in the buffer
    size_t head;                    // Index where the next sample will be written
    size_t tail;                    // Index of the oldest sample in the buffer
    T lastValue;                    // Caches the most recent value for immediate access
};

#endif // ROLLING_AVERAGE_H
