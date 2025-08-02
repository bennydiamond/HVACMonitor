#pragma once

/*
* These icons are defined using their direct UTF-8 byte sequences.
* This is the most reliable method for LVGL, as it avoids compiler
* issues with Unicode escape sequences like `\u`.
*/

// Gauge Icons (from mdi_44.c)
#define ICON_GAUGE_EMPTY   "\xF3\xB0\xA1\xB3"  // U+F0873
#define ICON_GAUGE_LOW     "\xF3\xB0\x8A\x9A"  // U+F029A
#define ICON_GAUGE         "\xF3\xB0\xA1\xB5"  // U+F0875
#define ICON_GAUGE_FULL    "\xF3\xB0\xA1\xB4"  // U+F0874

// Main Screen Icons (from mdi_30.c)
#define ICON_RADIOACTIVE   "\xF3\xB0\x90\xBC"  // U+F043C
#define ICON_THERMOMETER   "\xF3\xB0\x94\x8F"  // U+F050F
#define ICON_WATER_PERCENT "\xF3\xB0\x96\x8E"  // U+F058E
#define ICON_BLUR          "\xF3\xB0\x82\xB5"  // U+F00B5
#define ICON_MOLECULE_CO2  "\xF3\xB0\x9F\xA4"  // U+F07E4
#define ICON_LUNGS         "\xF3\xB1\x82\x84"  // U+F1084
#define ICON_MOLECULE_CO   "\xF3\xB1\x8B\xBE"  // U+F12FE
#define ICON_MOLECULE      "\xF3\xB0\xAE\xAC"  // U+F0BAC
#define ICON_CAR_SIDE      "\xF3\xB0\x9E\xAB"  // U+F07AB
#define ICON_SMOKE         "\xF3\xB1\x9E\x99"  // U+F1799
#define ICON_FAN           "\xF3\xB0\x88\x90"  // U+F0210
#define ICON_FAN_ALERT     "\xF3\xB1\x91\xAC"  // U+F146C
#define ICON_FAN_OFF       "\xF3\xB0\xA0\x9D"  // U+F081D
#define ICON_ENGINE        "\xF3\xB0\x87\xBA"  // U+F01FA
#define ICON_WATER_PUMP    "\xF3\xB1\x90\x82"  // U+F1402
#define ICON_WATER_ALERT "\xF3\xB1\x94\x82"    // U+F1502
#define ICON_WATER       "\xF3\xB0\x96\x8C"    // U+F058C

// Status Bar Icons (from mdi_30.c)
#define ICON_LAN_CONNECT        "\xF3\xB0\x8C\x98"  // U+F0318
#define ICON_LAN_DISCONNECT     "\xF3\xB0\x8C\x99"  // U+F0319
#define ICON_HOME_ASSISTANT     "\xF3\xB0\x9F\x90"  // U+F07D0
#define ICON_WIFI_ALERT         "\xF3\xB0\xA4\xAB"  // U+F092B
#define ICON_WIFI_STRENGTH_1    "\xF3\xB0\xA4\x9F"  // U+F091F
#define ICON_WIFI_STRENGTH_2    "\xF3\xB0\xA4\xA2"  // U+F0922
#define ICON_WIFI_STRENGTH_3    "\xF3\xB0\xA4\xA5"  // U+F0925
#define ICON_WIFI_STRENGTH_4    "\xF3\xB0\xA4\xA8"  // U+F0928
#define ICON_AIR_FILTER         "\xF3\xB0\xB5\x83"  // U+F0D43

// Debug screens
#define ICON_INFO               "\xF3\xB1\xB1\xAA"  // U+F1C6A
#define ICON_RESTART            "\xF3\xB1\x84\x8C"  // U+F110C
#define ICON_MEMORY             "\xF3\xB0\x8D\x9B"  // U+F035B
#define ICON_CLOCK              "\xF3\xB0\x85\x90"  // U+F0150
#define ICON_DATABASE           "\xF3\xB0\x86\xBC"  // U+F01BC
#define ICON_IP                 "\xF3\xB0\xB2\x90"  // U+F0C90
#define ICON_WIFI               "\xF3\xB0\x96\xA9"  // U+F05A9