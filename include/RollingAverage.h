#ifndef ROLLING_AVERAGE_H
#define ROLLING_AVERAGE_H

#include <Arduino.h>

template<typename T = float>
class RollingAverage {
public:
    /**
     * @brief Construct a new Rolling Average object.
     * @param num_samples The maximum number of samples to store in the history buffer.
     */
    explicit RollingAverage(size_t num_samples = 64)
        : max_samples(num_samples),
          history(new T[num_samples]),
          count(0),
          head(0),
          tail(0),
          lastValue(0),
          running_sum(0.0)
    {
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
     * Overwrites the oldest sample once the buffer capacity is reached.
     * Maintains running_sum for O(1) constant-time average computation.
     * @param newValue The new value to add.
     */
    void add(T newValue) {
        if (history == nullptr) return;

        // Reject NaN and Inf values to prevent poisoning running_sum
        double doubleVal = static_cast<double>(newValue);
        if (isnan(doubleVal) || isinf(doubleVal)) {
            return;
        }

        this->lastValue = newValue;

        // If buffer is already at max capacity, subtract the oldest value before overwriting
        if (count >= max_samples) {
            running_sum -= static_cast<double>(history[tail]);
            tail = (tail + 1) % max_samples;
            count--;
        }

        // Add the new sample at head
        history[head] = newValue;
        head = (head + 1) % max_samples;
        running_sum += doubleVal;
        count++;

        // Clean reset when empty
        if (count == 0) {
            running_sum = 0.0;
        } else if (head == 0) {
            // Periodic drift recalibration on ring buffer cycle
            double exact_sum = 0.0;
            size_t pos = tail;
            for (size_t i = 0; i < count; i++) {
                exact_sum += static_cast<double>(history[pos]);
                pos = (pos + 1) % max_samples;
            }
            running_sum = exact_sum;
        }
    }

    /**
     * @brief Calculates and returns the current average of the values in the buffer.
     * Runs in O(1) constant time using the tracked running sum.
     * @return The calculated rolling average.
     */
    T getAverage() const {
        if (history == nullptr || count == 0 || isnan(running_sum)) {
            return this->lastValue; // Return the last known value to avoid division by zero.
        }
        return static_cast<T>(running_sum / count);
    }

    /**
     * @brief Returns if the history buffer is empty.
     * @return True if the history buffer is empty, false otherwise.
     */
    bool isEmpty() const {
        return count == 0;
    }

    /**
     * @brief Returns the number of valid samples currently in the buffer.
     */
    size_t getCount() const {
        return count;
    }

    /**
     * @brief Clears the history buffer and resets the running sum.
     */
    void clear() {
        count = 0;
        head = 0;
        tail = 0;
        lastValue = 0; // getAverage() returns it when empty: never a stale reading
        running_sum = 0.0;
    }

private:
    size_t max_samples;             // The maximum number of samples for this instance
    T* history;                     // Pointer to the dynamically allocated history buffer
    size_t count;                   // Current number of valid samples in the buffer
    size_t head;                    // Index where the next sample will be written
    size_t tail;                    // Index of the oldest sample in the buffer
    T lastValue;                    // Caches the most recent value for immediate access
    double running_sum;             // Running sum of valid samples in history buffer
};

#endif // ROLLING_AVERAGE_H
