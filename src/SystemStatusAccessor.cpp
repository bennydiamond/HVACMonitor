#include "SystemStatusAccessor.h"

SemaphoreHandle_t SystemStatusAccessor::_sharedMutex = nullptr;
StaticSemaphore_t SystemStatusAccessor::_mutexBuffer;
SystemStatusData SystemStatusAccessor::_state;

void SystemStatusAccessor::init() {
    if (_sharedMutex == nullptr) {
        _sharedMutex = xSemaphoreCreateRecursiveMutexStatic(&_mutexBuffer);
    }
}

SystemStatusAccessor::SystemStatusAccessor()
    : BaseAccessor((init(), _sharedMutex)) {}

