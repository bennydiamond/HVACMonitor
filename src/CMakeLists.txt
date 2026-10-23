file(GLOB_RECURSE APP_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

idf_component_register(
    SRCS ${APP_SOURCES}
    INCLUDE_DIRS
        "."
        "../include"
        "../HVAC_NanoSensorHub/include"
        "zmod4510"
        "ui"
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        driver
        esp_timer
        esp_hw_support
        app_update
        mbedtls
        fatfs
        sdmmc
        vfs
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    -DBMP280_ENABLED
    -DSERIAL_BAUD_RATE=19200
    -DLV_LVGL_H_INCLUDE_SIMPLE
    -DARDUINOHA_MAXPROGMEM_STRINGS
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=format=
    -Wno-missing-field-initializers
    -Wno-unused-function
)

target_link_libraries(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/zmod4510/lib_no2_o3.a"
)
