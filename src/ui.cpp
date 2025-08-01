#include "ui.h"
#include "ui/custom_icons.h"

const lv_color_t COLOR_GREEN = lv_color_hex(0x7AE13B);
const lv_color_t COLOR_MID = lv_color_hex(0xFFC107);
const lv_color_t COLOR_HIGH = lv_color_hex(0xF44336);
const lv_color_t COLOR_HA_BLUE = lv_color_hex(0x41BDF5);
const lv_color_t COLOR_DISCONNECTED = lv_color_hex(0xF44336);
const lv_color_t COLOR_DEFAULT_ICON = lv_color_hex(0x9E9E9E);

LGFX* UI::_tft = nullptr;
CST820* UI::_touch = nullptr;
ConfigManager* UI::_config = nullptr;
lv_disp_draw_buf_t UI::draw_buf;
lv_color_t* UI::buf1 = nullptr;
lv_disp_drv_t UI::disp_drv;
lv_indev_drv_t UI::indev_drv;

UI* UI::_instance = nullptr;

// Private constructor implementation
UI::UI() {}

// Static method to get the singleton instance
UI& UI::getInstance() {
    if (!_instance) {
        _instance = new UI();
    }
    return *_instance;
}

void UI::init(LGFX* tft_instance, CST820* touch_instance, ConfigManager* config_instance) {
    _tft = tft_instance;
    _touch = touch_instance;
    _config = config_instance;

    lv_init();
    lv_disp_t * disp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE_GREY), lv_palette_main(LV_PALETTE_TEAL), 1, LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, theme);

    buf1 = (lv_color_t *)heap_caps_malloc(320 * 100 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 320 * 100);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read_cb;
    lv_indev_drv_register(&indev_drv);
}

void UI::create_widgets() {
    create_main_tileview();
    create_status_bar();
    
    tileManager = new UITileManager();
    tileManager->create_all_tiles(tileview, _config, fan_icon_label);

    lv_timer_create(inactivity_timer_cb_static, 1000, this);
    high_pressure_blink_timer = lv_timer_create(UIRuntimeTile::high_pressure_blink_cb, 750, air_filter_icon_label);
    lv_timer_pause(high_pressure_blink_timer);
}

void UI::run(void) {
    lv_timer_handler();
}

void UI::clearSensorReadings(void) { if (tileManager) tileManager->clear_all_readings(); }
void UI::update_pressure(float p) { if (tileManager) tileManager->update_pressure(p); }
void UI::update_geiger_cpm(int cpm) { if (tileManager) tileManager->update_geiger_cpm(cpm); }
void UI::update_geiger_usvh(float usv) { if (tileManager) tileManager->update_geiger_usvh(usv); }
void UI::update_temp(float temp) { if (tileManager) tileManager->update_temp(temp); }
void UI::update_humi(float humi) { if (tileManager) tileManager->update_humi(humi); }
void UI::update_pm1(float pm1) { if (tileManager) tileManager->update_pm1(pm1); }
void UI::update_pm25(float pm25) { if (tileManager) tileManager->update_pm25(pm25); }
void UI::update_pm4(float pm4) { if (tileManager) tileManager->update_pm4(pm4); }
void UI::update_pm10(float pm10) { if (tileManager) tileManager->update_pm10(pm10); }
void UI::update_fan_amps(float amps) { if (tileManager) tileManager->update_fan_amps(amps); }
void UI::update_compressor_amps(float amps) { if (tileManager) tileManager->update_compressor_amps(amps); }
void UI::update_pump_amps(float amps) { if (tileManager) tileManager->update_pump_amps(amps); }
void UI::update_water_sensor(bool water_ok) { if (tileManager) tileManager->update_water_sensor(water_ok); }
void UI::update_fan_status(FanStatus s) { if (tileManager) tileManager->update_fan_status(s); }
void UI::update_co2(float co2) { if (tileManager) tileManager->update_co2(co2); }
void UI::update_voc(int32_t voc_index) { if (tileManager) tileManager->update_voc(voc_index); }
void UI::update_no2(float no2_ppb) { if (tileManager) tileManager->update_no2(no2_ppb); }
void UI::update_o3(float o3_ppb) { if (tileManager) tileManager->update_o3(o3_ppb); }
void UI::update_nox(float nox_ppb) { if (tileManager) tileManager->update_nox(nox_ppb); }
void UI::update_co(float co_ppm) { if (tileManager) tileManager->update_co(co_ppm); }
void UI::set_initial_debug_info(const char* v, const char* r) { if (tileManager) tileManager->set_initial_debug_info(v, r); }
void UI::update_scd30_autocal(bool enabled) { if (tileManager) tileManager->update_scd30_autocal(enabled); }
void UI::update_scd30_forcecal(uint16_t ppm) { if (tileManager) tileManager->update_scd30_forcecal(ppm); }
void UI::update_sps30_fan_interval(unsigned long v) { if (tileManager) tileManager->update_sps30_fan_interval(v); }
void UI::update_sps30_fan_days(unsigned long v) { if (tileManager) tileManager->update_sps30_fan_days(v); }
void UI::update_sgp41_test_status(int v) { if (tileManager) tileManager->update_sgp41_test_status(v); }
void UI::update_sgp41_test_value(uint16_t v) { if (tileManager) tileManager->update_sgp41_test_value(v); }
void UI::update_runtime_free_heap(uint32_t v) { if (tileManager) tileManager->update_runtime_free_heap(v); }
void UI::update_runtime_uptime(unsigned long v) { if (tileManager) tileManager->update_runtime_uptime(v); }
void UI::update_sensorstack_uptime(uint32_t v) { if (tileManager) tileManager->update_sensorstack_uptime(v); }
void UI::update_sensorstack_ram(uint16_t v) { if (tileManager) tileManager->update_sensorstack_ram(v); }
void UI::update_network_rssi(int8_t v) { if (tileManager) tileManager->update_network_rssi(v); }
void UI::update_network_ha_conn(bool v) { if (tileManager) tileManager->update_network_ha_conn(v); update_ha_status(v); }
void UI::update_last_packet_time(uint32_t v) { if (tileManager) tileManager->update_last_packet_time(v); }
void UI::update_ssid(const char* ssid) { if (tileManager) tileManager->update_ssid(ssid); }
void UI::update_ip(const char* ip) { if (tileManager) tileManager->update_ip(ip); }
void UI::update_mac(const char* mac) { if (tileManager) tileManager->update_mac(mac); }
void UI::update_fw_version(const char* fw) { if (tileManager) tileManager->update_fw_version(fw); }

void UI::create_status_bar() {
    status_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(status_bar, 35, 240);
    lv_obj_align(status_bar, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_move_to_index(status_bar, -1);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(status_bar, 20, 0);
    lv_obj_set_style_pad_ver(status_bar, 10, 0);
    lv_obj_set_scroll_dir(status_bar, LV_DIR_NONE);

    fan_icon_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(fan_icon_label, &mdi_30, 0);
    lv_label_set_text(fan_icon_label, ICON_FAN);
    lv_obj_set_style_text_color(fan_icon_label, COLOR_GREEN, 0);
    lv_obj_add_flag(fan_icon_label, LV_OBJ_FLAG_HIDDEN);
    
    air_filter_icon_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(air_filter_icon_label, &mdi_30, 0);
    lv_label_set_text(air_filter_icon_label, ICON_AIR_FILTER);
    lv_obj_set_style_text_color(air_filter_icon_label, COLOR_HIGH, 0);
    lv_obj_add_flag(air_filter_icon_label, LV_OBJ_FLAG_HIDDEN);

    wifi_icon_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(wifi_icon_label, &mdi_30, 0);
    lv_label_set_text(wifi_icon_label, ICON_WIFI_ALERT);
    
    ha_icon_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(ha_icon_label, &mdi_30, 0);
    lv_label_set_text(ha_icon_label, ICON_HOME_ASSISTANT);
    lv_obj_set_style_text_color(ha_icon_label, COLOR_DISCONNECTED, 0);
    
    sensor_icon_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(sensor_icon_label, &mdi_30, 0);
    lv_label_set_text(sensor_icon_label, ICON_LAN_DISCONNECT);
    lv_obj_set_style_text_color(sensor_icon_label, COLOR_DISCONNECTED, 0);
}

void UI::create_main_tileview() {
    tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(tileview, 320, 240);
    lv_obj_align(tileview, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(tileview, 5, 0);
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(tileview, LV_DIR_VER | LV_DIR_RIGHT);
    lv_obj_add_event_cb(tileview, tile_change_event_cb_static, LV_EVENT_VALUE_CHANGED, this);
}

void UI::update_high_pressure_status(bool is_high) {
    if (is_high) {
        lv_obj_clear_flag(air_filter_icon_label, LV_OBJ_FLAG_HIDDEN);
        if(high_pressure_blink_timer && high_pressure_blink_timer->paused) {
            lv_timer_reset(high_pressure_blink_timer);
            lv_timer_resume(high_pressure_blink_timer);
        }
    } else {
        if(high_pressure_blink_timer) lv_timer_pause(high_pressure_blink_timer);
        lv_obj_set_style_opa(air_filter_icon_label, LV_OPA_COVER, 0);
        lv_obj_add_flag(air_filter_icon_label, LV_OBJ_FLAG_HIDDEN);
    }
}
void UI::update_wifi_status(bool connected, int8_t rssi) {
    if (connected) {
        lv_color_t color;
        const char* icon;
        if (rssi >= -67) { color = COLOR_GREEN; icon = ICON_WIFI_STRENGTH_4; } 
        else if (rssi >= -75) { color = COLOR_GREEN; icon = ICON_WIFI_STRENGTH_3; } 
        else if (rssi >= -82) { color = COLOR_MID; icon = ICON_WIFI_STRENGTH_2; } 
        else { color = COLOR_MID; icon = ICON_WIFI_STRENGTH_1; }
        lv_label_set_text(wifi_icon_label, icon);
        lv_obj_set_style_text_color(wifi_icon_label, color, 0);
    } else {
        lv_label_set_text(wifi_icon_label, ICON_WIFI_ALERT);
        lv_obj_set_style_text_color(wifi_icon_label, COLOR_DISCONNECTED, 0);
    }
}
void UI::update_ha_status(bool connected) {
    lv_obj_set_style_text_color(ha_icon_label, connected ? COLOR_HA_BLUE : COLOR_DISCONNECTED, 0);
}
void UI::update_sensor_status(bool connected) {
    lv_label_set_text(sensor_icon_label, connected ? ICON_LAN_CONNECT : ICON_LAN_DISCONNECT);
    lv_obj_set_style_text_color(sensor_icon_label, connected ? COLOR_GREEN : COLOR_DISCONNECTED, 0);
    if (tileManager) tileManager->update_sensor_status(connected);
}

void UI::disp_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    if (_tft == nullptr) return;
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    _tft->startWrite();
    _tft->setAddrWindow(area->x1, area->y1, w, h);
    _tft->writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
    _tft->endWrite();
    lv_disp_flush_ready(disp);
}

void UI::touchpad_read_cb(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    if (_touch == nullptr) return;
    static uint16_t last_x = 0, last_y = 0;
    uint16_t touchX, touchY;
    uint8_t gesture;
    bool touched = _touch->getTouch(&touchX, &touchY, &gesture);
    if (touched) {
        uint16_t rotatedX = touchY;
        uint16_t rotatedY = 239 - touchX;
        last_x = rotatedX; 
        last_y = rotatedY;
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_REL;
    }
}

void UI::inactivity_timer_cb_static(lv_timer_t *timer) {
    UI* ui = static_cast<UI*>(timer->user_data);
    if (ui) ui->inactivity_timer_cb(timer);
}

void UI::inactivity_timer_cb(lv_timer_t *timer) {
    if (lv_disp_get_inactive_time(NULL) > 30000) {
        if (tileManager && lv_tileview_get_tile_act(tileview) != tileManager->get_tile1()) {
            lv_obj_set_tile_id(tileview, 0, 0, LV_ANIM_ON);
        }
    }
}

void UI::tile_change_event_cb_static(lv_event_t * e) {
    UI* ui_instance = (UI*)lv_event_get_user_data(e);
    if (ui_instance) ui_instance->tile_change_event_cb(e);
}

void UI::tile_change_event_cb(lv_event_t * e) {
    lv_obj_t * active_tile = lv_tileview_get_tile_act(tileview);
    
    // Clear SGP41 test results when leaving SGP41 tile
    static lv_obj_t* prev_tile = nullptr;
    if (tileManager && prev_tile == tileManager->get_sgp41_tile() && active_tile != prev_tile) {
        tileManager->clear_sgp41_results();
    }
    prev_tile = active_tile;
    
    if (tileManager && (active_tile == tileManager->get_tile1() || active_tile == tileManager->get_secondary_tile() || active_tile == tileManager->get_gas_tile() || active_tile == tileManager->get_powerdraw_tile())) {
        // Show status bar for first column tiles
        if(status_bar) lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_HIDDEN);
        // Force grid layout refresh to maintain proper alignment
        lv_obj_mark_layout_as_dirty(active_tile);
    } else {
        // Hide status bar for other columns
        if(status_bar) lv_obj_add_flag(status_bar, LV_OBJ_FLAG_HIDDEN);
    }
}
