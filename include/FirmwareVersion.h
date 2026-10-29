#pragma once

// Firmware version reported everywhere (screen, web, MQTT, syslog), e.g.
// "1.1.1 (a1b2c3d+)": HVACMONITOR_FIRMWARE_VERSION, then the git hash of the build,
// with '+' when it was built with uncommitted changes
const char* firmwareVersion();

// The git hash alone, e.g. "a1b2c3d+"
const char* firmwareGitHash();
