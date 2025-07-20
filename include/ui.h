#ifndef UI_H
#define UI_H

#include "ui/interfaces/IUIManager.h"
#include "ui/interfaces/IUIUpdater.h"

#include <lvgl.h>
#include "LGFX_ESP32_2432S022C.h"
#include "CST820.h"
#include "ConfigManager.h"
#include "ui/ui_common.h"
#include "ui/ui_tile_manager.h"

class UI : public IUIManager, public IUIUpdater {
public:
    // Static method to get the singleton instance
    static UI& getInstance();

    // --- IUIManager Implementation ---
    void init(LGFX* tft, CST820* touch, ConfigManager* config) override;
    void create_widgets(void) override;
    void run(void) override;
    // --- IUIUpdater Implementation ---
    void clearSensorReadings(void) override;
    void update_pressure(float p) override;
    void update_geiger_reading(int cpm, float usv) override;
    void update_temp_humi(float temp, float humi) override;
    void update_pm_values(float pm1, float pm25, float pm4, float pm10) override;
    void update_fan_current(float amps, FanStatus status) override;
    void update_fan_status(FanStatus status) override;
    void update_high_pressure_status(bool is_high) override;
    void update_wifi_status(bool connected, int8_t rssi) override;
    void update_ha_status(bool connected) override;
    void update_sensor_status(bool connected) override;
    void update_co2(float co2_ppm) override;
    void update_voc(int32_t voc_index) override;
    void set_initial_debug_info(const char* version, const char* reason) override;
    void update_runtime_info(uint32_t freemem, unsigned long uptime_ms) override;
    void update_network_info(const char* ip, const char* mac, int8_t rssi, const char* ssid, bool ha_connected) override;
    void update_last_packet_time(uint32_t seconds_since_packet, bool connected) override;
    void update_sensorstack_info(const char* version, uint32_t uptime, uint16_t free_ram, bool connected) override;
    void update_scd30_autocal(bool enabled) override;
    void update_scd30_forcecal(uint16_t ppm) override;
    void update_sps30_info(uint32_t fan_interval, uint8_t fan_days) override;
    void update_sgp41_test(int result, uint16_t value) override;

private:
    static LGFX* _tft;
    static CST820* _touch;
    static ConfigManager* _config;
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t* buf1;
    static lv_disp_drv_t disp_drv;
    static lv_indev_drv_t indev_drv;

    // Private constructor to prevent instantiation
    UI();
    // Delete copy constructor and assignment operator
    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;
    static void disp_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void touchpad_read_cb(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
    void create_status_bar(void);
    void create_main_tileview(void);
    static void inactivity_timer_cb_static(lv_timer_t *timer);
    void inactivity_timer_cb(lv_timer_t *timer);
    static void tile_change_event_cb_static(lv_event_t * e);
    void tile_change_event_cb(lv_event_t * e);

    // Static instance pointer
    static UI* _instance;
    lv_obj_t* tileview = nullptr;
    lv_obj_t* status_bar = nullptr;
    UITileManager* tileManager = nullptr;
    lv_obj_t* fan_icon_label = nullptr;
    lv_obj_t* air_filter_icon_label = nullptr;
    lv_obj_t* wifi_icon_label = nullptr;
    lv_obj_t* ha_icon_label = nullptr;
    lv_obj_t* sensor_icon_label = nullptr;
    lv_timer_t* high_pressure_blink_timer = nullptr;
};

#endif // UI_H