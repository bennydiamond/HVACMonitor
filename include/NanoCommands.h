#pragma once

// --- Protocol Definitions ---
#define CMD_BROADCAST_SENSORS 'S'
#define CMD_GET_VERSION       'V'
#define CMD_GET_HEALTH        'H'
#define RSP_VERSION           'v'
#define CMD_ACK_HEALTH        'A' // Acknowledge Health Status
#define CMD_REBOOT            'R' // Request Nano Reboot
#define RSP_HEALTH            'h'
#define CMD_GET_SPS30_INFO    'P' // Request SPS30 info
#define RSP_SPS30_INFO        'p' // Response with SPS30 info
#define CMD_SGP40_TEST        'G' // Request SGP40 test
#define RSP_SGP40_TEST        'g' // Response with SGP40 test result

// Function to send a command to the Nano, defined in main.cpp
void send_command_to_nano(char cmd);