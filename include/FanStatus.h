#pragma once
#include <cstdint>

enum class FanStatus : uint8_t {
    Off = 0,
    Normal = 1,
    Alert = 2
};

constexpr FanStatus FAN_STATUS_OFF = FanStatus::Off;
constexpr FanStatus FAN_STATUS_NORMAL = FanStatus::Normal;
constexpr FanStatus FAN_STATUS_ALERT = FanStatus::Alert;

