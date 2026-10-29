#include "FirmwareVersion.h"
#include "HVACMonitor.h"
#include "GitVersion.h"

const char* firmwareVersion() {
    return HVACMONITOR_FIRMWARE_VERSION " (" HVACMONITOR_GIT_HASH ")";
}

const char* firmwareGitHash() {
    return HVACMONITOR_GIT_HASH;
}
