#include "BaseAccessor.h"

BaseAccessor::BaseAccessor(SemaphoreHandle_t mutex, TickType_t timeout)
    : _mutex(mutex), _locked(false) {
    if (_mutex != nullptr) {
        _locked = (xSemaphoreTakeRecursive(_mutex, timeout) == pdTRUE);
    }
}

BaseAccessor::~BaseAccessor() {
    if (_locked && _mutex != nullptr) {
        xSemaphoreGiveRecursive(_mutex);
        _locked = false;
    }
}

BaseAccessor::BaseAccessor(BaseAccessor&& other) noexcept
    : _mutex(other._mutex), _locked(other._locked) {
    other._mutex = nullptr;
    other._locked = false;
}

BaseAccessor& BaseAccessor::operator=(BaseAccessor&& other) noexcept {
    if (this != &other) {
        if (_locked && _mutex != nullptr) {
            xSemaphoreGiveRecursive(_mutex);
        }
        _mutex = other._mutex;
        _locked = other._locked;
        other._mutex = nullptr;
        other._locked = false;
    }
    return *this;
}
