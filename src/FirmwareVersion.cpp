#include "FirmwareVersion.h"
#include "HVACMonitor.h"
#include "GitVersion.h"

const char* firmwareVersion() {
    return HVACMONITOR_FIRMWARE_VERSION HVACMONITOR_VERSION_SUFFIX;
}
