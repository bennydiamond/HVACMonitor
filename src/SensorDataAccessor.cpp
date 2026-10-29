#include "SensorDataAccessor.h"

SemaphoreHandle_t SensorDataAccessor::_sharedMutex = nullptr;
StaticSemaphore_t SensorDataAccessor::_mutexBuffer;
SensorStateData SensorDataAccessor::_state;

void SensorDataAccessor::init() {
    if (_sharedMutex == nullptr) {
        _sharedMutex = xSemaphoreCreateRecursiveMutexStatic(&_mutexBuffer);
    }
}

SensorDataAccessor::SensorDataAccessor()
    : BaseAccessor((init(), _sharedMutex)) {}

