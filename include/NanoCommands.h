#pragma once

// --- Protocol Definitions ---
#define CMD_BROADCAST_SENSORS 'S'
#define CMD_GET_VERSION       'V'
#define CMD_GET_HEALTH        'H'
#define RSP_HEALTH            'h' // Overall health status response
#define CMD_ACK_HEALTH        'A' // Acknowledge Health Status
#define RSP_VERSION           'v'
#define CMD_REBOOT            'R' // Request Nano Reboot
#define CMD_GET_SPS30_INFO    'P' // Request SPS30 info
#define RSP_SPS30_INFO        'p' // Response with SPS30 info
#define CMD_SPS30_CLEAN       'C' // Request manual SPS30 fan cleaning
#define RSP_SPS30_CLEAN       'c' // ACK for fan cleaning command
#define CMD_SGP40_TEST        'G' // Request SGP40 test
#define RSP_SGP40_TEST        'g' // Response with SGP40 test result
#define CMD_GET_SCD30_INFO    'D' // Request SCD30 info
#define RSP_SCD30_INFO        'd' // Response with SCD30 info
#define CMD_SET_SCD30_AUTOCAL 'T' // Set SCD30 AutoCalibration
#define RSP_SCD30_AUTOCAL     't' // Response with SCD30 AutoCalibration result
#define CMD_SET_SCD30_FORCECAL 'F' // Set SCD30 Forced Recalibration
#define RSP_SCD30_FORCECAL     'f' // Response with SCD30 Forced Recalibration result

// Function to send a command to the Nano, defined in main.cpp
void send_command_to_nano(char cmd);