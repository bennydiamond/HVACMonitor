#pragma once

// Firmware version reported everywhere (screen, web, MQTT, syslog):
// HVACMONITOR_FIRMWARE_VERSION, then "-<git hash>" of the build, with '+' when it
// was built with uncommitted changes, e.g. "1.1.1-a1b2c3d+". A clean build of a
// tagged commit is a release: the version alone, e.g. "1.1.1".
const char* firmwareVersion();
