#include "DiagnosticDataAccessor.h"

SemaphoreHandle_t DiagnosticDataAccessor::_sharedMutex = nullptr;
StaticSemaphore_t DiagnosticDataAccessor::_mutexBuffer;
DiagnosticData DiagnosticDataAccessor::_state;

void DiagnosticDataAccessor::init() {
    if (_sharedMutex == nullptr) {
        _sharedMutex = xSemaphoreCreateRecursiveMutexStatic(&_mutexBuffer);
    }
}

DiagnosticDataAccessor::DiagnosticDataAccessor()
    : BaseAccessor((init(), _sharedMutex)) {}

