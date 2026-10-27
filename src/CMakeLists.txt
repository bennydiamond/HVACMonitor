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

# Compress web assets before compiling source files in ESP-IDF / lean_idf5
set(WEB_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../web")
set(WEB_ASSETS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/compress_web_assets.py")
set(WEB_ASSETS_GZ_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/../include/webserver/WebServerAssetsGz.h")

if(NOT python)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(python ${Python3_EXECUTABLE})
endif()

add_custom_command(
    OUTPUT "${WEB_ASSETS_GZ_HEADER}"
    COMMAND ${python} "${WEB_ASSETS_SCRIPT}" "${CMAKE_CURRENT_SOURCE_DIR}/.."
    DEPENDS
        "${WEB_ASSETS_SCRIPT}"
        "${WEB_ASSETS_DIR}/style.css"
        "${WEB_ASSETS_DIR}/app.js"
    COMMENT "Compressing web assets for WebServerManager"
    VERBATIM
)

add_custom_target(compress_web_assets DEPENDS "${WEB_ASSETS_GZ_HEADER}")
add_dependencies(${COMPONENT_LIB} compress_web_assets)
