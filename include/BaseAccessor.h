#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @brief Base RAII accessor class that takes a FreeRTOS recursive mutex in its
 *        constructor and releases it in its destructor.
 */
class BaseAccessor {
public:
    explicit BaseAccessor(SemaphoreHandle_t mutex, TickType_t timeout = portMAX_DELAY);
    virtual ~BaseAccessor();

    // Disable copy semantics
    BaseAccessor(const BaseAccessor&) = delete;
    BaseAccessor& operator=(const BaseAccessor&) = delete;

    // Move semantics allowed
    BaseAccessor(BaseAccessor&& other) noexcept;
    BaseAccessor& operator=(BaseAccessor&& other) noexcept;

    bool isLocked() const { return _locked; }

protected:
    SemaphoreHandle_t _mutex = nullptr;
    bool _locked = false;
};
