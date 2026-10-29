#pragma once
#include <cstdint>

enum class SensorStackFirmwareState : uint8_t {
    Ok = 0,
    Updating = 1,
    Faulty = 2,
};

