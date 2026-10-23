cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXCLUDE_COMPONENTS
    bt
    espressif__esp-modbus
    espressif__esp_insights
    espressif__esp_diagnostics
    espressif__esp_diag_data_store
    espressif__esp_rainmaker
    espressif__rmaker_common
    espressif__rmaker_cmd_resp
    espressif__rmaker_common_events
    espressif__rmaker_console
    espressif__rmaker_system_ctrl
    espressif__rmaker_time_sync
    espressif__rmaker_work_queue
    espressif__esp_daylight
    espressif__esp_schedule
    espressif__esp_secure_cert_mgr
    espressif__esp-zboss-lib
    espressif__esp-zigbee-lib
    espressif__esp_modem
    chmorgan__esp-libhelix-mp3
    espressif__lan867x
    espressif__lan86xx_common
)

project(HVACMonitor)


