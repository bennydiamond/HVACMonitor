#include "ui.h"
#include "ConfigManager.h"
#include "ui/custom_icons.h"


const lv_color_t COLOR_GREEN = lv_color_hex(0x7AE13B);
const lv_color_t COLOR_MID = lv_color_hex(0xFFC107);
const lv_color_t COLOR_HIGH = lv_color_hex(0xF44336);
const lv_color_t COLOR_HA_BLUE = lv_color_hex(0x41BDF5);
const lv_color_t COLOR_DISCONNECTED = lv_color_hex(0xF44336);
const lv_color_t COLOR_DEFAULT_ICON = lv_color_hex(0x9E9E9E);

LGFX *UI::_tft = nullptr;
CST820 *UI::_touch = nullptr;
lv_disp_draw_buf_t UI::draw_buf;
lv_color_t *UI::buf1 = nullptr;
lv_disp_drv_t UI::disp_drv;
lv_indev_drv_t UI::indev_drv;
lv_color_t *UI::buf2 = nullptr;

// Lines per draw buffer. Two buffers are used so rendering overlaps DMA
// transfer.
static constexpr uint32_t DRAW_BUF_LINES = 40;
static constexpr uint32_t DRAW_BUF_PIXELS = 320 * DRAW_BUF_LINES;

UI::UI()
    : tileview(nullptr), status_bar(nullptr), tileManager(nullptr),
      fan_icon_label(nullptr), air_filter_icon_label(nullptr),
      wifi_icon_label(nullptr), ha_icon_label(nullptr),
      sensor_icon_label(nullptr), high_pressure_blink_timer(nullptr) {
  logger.info("UI top class created");
}

UI::~UI() {
  delete tileManager;

  lv_obj_del(tileview);
  lv_obj_del(fan_icon_label);
  lv_obj_del(wifi_icon_label);
  lv_obj_del(ha_icon_label);
  lv_obj_del(sensor_icon_label);
  lv_obj_del(air_filter_icon_label);
  lv_obj_del(status_bar);
  lv_timer_del(high_pressure_blink_timer);
  lv_timer_del(sensor_firmware_blink_timer);

  if (_tft) {
    _tft->waitDMA();
  }
  if (buf1) {
    heap_caps_free(buf1);
    buf1 = nullptr;
  }
  if (buf2) {
    heap_caps_free(buf2);
    buf2 = nullptr;
  }
  logger.info("UI top class destroyed");
}

void UI::init(LGFX *tft_instance, CST820 *touch_instance) {
  _tft = tft_instance;
  _touch = touch_instance;

  lv_init();
  lv_disp_t *disp = lv_disp_get_default();
  lv_theme_t *theme = lv_theme_default_init(
      disp, lv_palette_main(LV_PALETTE_BLUE_GREY),
      lv_palette_main(LV_PALETTE_TEAL), 1, LV_FONT_DEFAULT);
  lv_disp_set_theme(disp, theme);

  buf1 = (lv_color_t *)heap_caps_malloc(DRAW_BUF_PIXELS * sizeof(lv_color_t),
                                        MALLOC_CAP_DMA);
  buf2 = (lv_color_t *)heap_caps_malloc(DRAW_BUF_PIXELS * sizeof(lv_color_t),
                                        MALLOC_CAP_DMA);
  assert(buf1 && buf2);
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DRAW_BUF_PIXELS);

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
  tileManager->create_all_tiles(tileview, fan_icon_label);

  // Set initial tile visibility (main tile and adjacent tiles visible)
  tileManager->update_tile_visibility(tileManager->get_tile1());

  lv_timer_create(inactivity_timer_cb_static, 1000, this);
  high_pressure_blink_timer = lv_timer_create(
      UIRuntimeTile::high_pressure_blink_cb, 750, air_filter_icon_label);
  lv_timer_pause(high_pressure_blink_timer);
  // Same opacity toggle as the high pressure icon
  sensor_firmware_blink_timer = lv_timer_create(
      UIRuntimeTile::high_pressure_blink_cb, 750, sensor_icon_label);
  lv_timer_pause(sensor_firmware_blink_timer);
}

uint32_t UI::run(void) { return lv_timer_handler(); }

void UI::clearSensorReadings(void) {
  if (tileManager)
    tileManager->clear_all_readings();
}
void UI::update_pressure(float p) {
  if (tileManager)
    tileManager->update_pressure(p);
}
void UI::update_geiger_cpm(int cpm) {
  if (tileManager)
    tileManager->update_geiger_cpm(cpm);
}
void UI::update_geiger_usvh(float usv) {
  if (tileManager)
    tileManager->update_geiger_usvh(usv);
}
void UI::update_temp(float temp) {
  if (tileManager)
    tileManager->update_temp(temp);
}
void UI::update_humi(float humi) {
  if (tileManager)
    tileManager->update_humi(humi);
}
void UI::update_pm1(float pm1) {
  if (tileManager)
    tileManager->update_pm1(pm1);
}
void UI::update_pm25(float pm25) {
  if (tileManager)
    tileManager->update_pm25(pm25);
}
void UI::update_pm4(float pm4) {
  if (tileManager)
    tileManager->update_pm4(pm4);
}
void UI::update_pm10(float pm10) {
  if (tileManager)
    tileManager->update_pm10(pm10);
}
void UI::update_fan_amps(float amps) {
  if (tileManager)
    tileManager->update_fan_amps(amps);
}
void UI::update_compressor_amps(float amps) {
  if (tileManager)
    tileManager->update_compressor_amps(amps);
}
void UI::update_pump_amps(float amps) {
  if (tileManager)
    tileManager->update_pump_amps(amps);
}
void UI::update_water_sensor(bool water_detected) {
  if (tileManager)
    tileManager->update_water_sensor(water_detected);
}
void UI::update_fan_status(FanStatus s) {
  if (tileManager)
    tileManager->update_fan_status(s);
}
void UI::update_co2(float co2) {
  if (tileManager)
    tileManager->update_co2(co2);
}
void UI::update_voc(int32_t voc_index) {
  if (tileManager)
    tileManager->update_voc(voc_index);
}
void UI::update_no2(float no2_ppb) {
  if (tileManager)
    tileManager->update_no2(no2_ppb);
}
void UI::update_o3(float o3_ppb) {
  if (tileManager)
    tileManager->update_o3(o3_ppb);
}
void UI::update_nox(float nox_ppb) {
  if (tileManager)
    tileManager->update_nox(nox_ppb);
}
void UI::update_co(float co_ppm) {
  if (tileManager)
    tileManager->update_co(co_ppm);
}
void UI::set_initial_debug_info(const char *v, const char *r) {
  if (tileManager)
    tileManager->set_initial_debug_info(v, r);
}
void UI::update_scd30_autocal(bool enabled) {
  if (tileManager)
    tileManager->update_scd30_autocal(enabled);
}
void UI::update_scd30_forcecal(uint16_t ppm) {
  if (tileManager)
    tileManager->update_scd30_forcecal(ppm);
}
void UI::update_sps30_fan_interval(unsigned long v) {
  if (tileManager)
    tileManager->update_sps30_fan_interval(v);
}
void UI::update_sps30_fan_days(unsigned long v) {
  if (tileManager)
    tileManager->update_sps30_fan_days(v);
}
void UI::update_sgp41_test_status(int v) {
  if (tileManager)
    tileManager->update_sgp41_test_status(v);
}
void UI::update_sgp41_test_value(uint16_t v) {
  if (tileManager)
    tileManager->update_sgp41_test_value(v);
}
void UI::update_runtime_free_heap(uint32_t v) {
  if (tileManager)
    tileManager->update_runtime_free_heap(v);
}
void UI::update_runtime_uptime(unsigned long v) {
  if (tileManager)
    tileManager->update_runtime_uptime(v);
}
void UI::update_sensorstack_uptime(uint32_t v) {
  if (tileManager)
    tileManager->update_sensorstack_uptime(v);
}
void UI::update_sensorstack_ram(uint16_t v) {
  if (tileManager)
    tileManager->update_sensorstack_ram(v);
}
void UI::update_network_rssi(int8_t v) {
  if (tileManager)
    tileManager->update_network_rssi(v);
}
void UI::update_network_ha_conn(bool v) {
  if (tileManager)
    tileManager->update_network_ha_conn(v);
  update_ha_status(v);
}
void UI::update_last_packet_time(uint32_t v) {
  if (tileManager)
    tileManager->update_last_packet_time(v);
}
void UI::update_ssid(const char *ssid) {
  if (tileManager)
    tileManager->update_ssid(ssid);
}
void UI::update_ip(const char *ip) {
  if (tileManager)
    tileManager->update_ip(ip);
}
void UI::update_mac(const char *mac) {
  if (tileManager)
    tileManager->update_mac(mac);
}
void UI::update_fw_version(const char *fw) {
  if (tileManager)
    tileManager->update_fw_version(fw);
}

void UI::create_status_bar() {
  status_bar = lv_obj_create(lv_scr_act());
  lv_obj_set_size(status_bar, 35, 240);
  lv_obj_align(status_bar, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_move_to_index(status_bar, -1);
  lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
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
  lv_obj_add_event_cb(tileview, tile_change_event_cb_static,
                      LV_EVENT_VALUE_CHANGED, this);
}

void UI::update_high_pressure_status(bool is_high) {
  if (is_high) {
    lv_obj_clear_flag(air_filter_icon_label, LV_OBJ_FLAG_HIDDEN);
    if (high_pressure_blink_timer && high_pressure_blink_timer->paused) {
      lv_timer_reset(high_pressure_blink_timer);
      lv_timer_resume(high_pressure_blink_timer);
    }
  } else {
    if (high_pressure_blink_timer)
      lv_timer_pause(high_pressure_blink_timer);
    lv_obj_set_style_opa(air_filter_icon_label, LV_OPA_COVER, 0);
    lv_obj_add_flag(air_filter_icon_label, LV_OBJ_FLAG_HIDDEN);
  }
}
void UI::update_wifi_status(bool connected, int8_t rssi) {
  if (connected) {
    lv_color_t color;
    const char *icon;
    if (rssi >= -67) {
      color = COLOR_GREEN;
      icon = ICON_WIFI_STRENGTH_4;
    } else if (rssi >= -75) {
      color = COLOR_GREEN;
      icon = ICON_WIFI_STRENGTH_3;
    } else if (rssi >= -82) {
      color = COLOR_MID;
      icon = ICON_WIFI_STRENGTH_2;
    } else {
      color = COLOR_MID;
      icon = ICON_WIFI_STRENGTH_1;
    }
    ui_set_label_text(wifi_icon_label, icon);
    ui_set_text_color(wifi_icon_label, color);
  } else {
    ui_set_label_text(wifi_icon_label, ICON_WIFI_ALERT);
    ui_set_text_color(wifi_icon_label, COLOR_DISCONNECTED);
  }
}
void UI::update_ha_status(bool connected) {
  ui_set_text_color(ha_icon_label,
                    connected ? COLOR_HA_BLUE : COLOR_DISCONNECTED);
}
void UI::update_sensor_status(bool connected) {
  sensor_connected = connected;
  refresh_sensor_icon();
  if (tileManager)
    tileManager->update_sensor_status(connected);
}

void UI::refresh_sensor_icon(void) {
  // A SensorStack firmware issue (state 2) overrides the link icon and blinks
  if (sensorstack_firmware_state == 2) {
    ui_set_label_text(sensor_icon_label, ICON_MEMORY_ARROW_DOWN);
    ui_set_text_color(sensor_icon_label, COLOR_HIGH);
    if (sensor_firmware_blink_timer && sensor_firmware_blink_timer->paused) {
      lv_timer_reset(sensor_firmware_blink_timer);
      lv_timer_resume(sensor_firmware_blink_timer);
    }
    return;
  }

  if (sensor_firmware_blink_timer)
    lv_timer_pause(sensor_firmware_blink_timer);
  lv_obj_set_style_opa(sensor_icon_label, LV_OPA_COVER, 0);
  ui_set_label_text(sensor_icon_label,
                    sensor_connected ? ICON_LAN_CONNECT : ICON_LAN_DISCONNECT);
  // Linked but with a defective sensor: orange instead of green
  lv_color_t color = !sensor_connected            ? COLOR_DISCONNECTED
                     : sensorstack_sensor_faults ? COLOR_MID
                                                 : COLOR_GREEN;
  ui_set_text_color(sensor_icon_label, color);
}

void UI::update_sensorstack_sensor_faults(int mask) {
  sensorstack_sensor_faults = mask;
  refresh_sensor_icon();
  if (tileManager)
    tileManager->update_sensorstack_sensor_faults(mask);
}

void UI::create_firmware_popup(void) {
  // Full-screen dimmed layer: absorbs touches so the tiles can't be swiped underneath
  firmware_popup = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(firmware_popup);
  lv_obj_set_size(firmware_popup, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(firmware_popup, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(firmware_popup, LV_OPA_70, 0);
  lv_obj_add_flag(firmware_popup, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(firmware_popup, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(firmware_popup, firmware_popup_clicked_cb, LV_EVENT_CLICKED, this);

  lv_obj_t *panel = lv_obj_create(firmware_popup);
  lv_obj_set_size(panel, 300, LV_SIZE_CONTENT);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x202020), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 12, 0);
  lv_obj_set_style_pad_all(panel, 12, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  firmware_popup_icon = lv_label_create(panel);
  lv_obj_set_style_text_font(firmware_popup_icon, &mdi_30, 0);

  firmware_popup_label = lv_label_create(panel);
  lv_obj_set_width(firmware_popup_label, LV_PCT(100));
  lv_label_set_long_mode(firmware_popup_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(firmware_popup_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(firmware_popup_label, lv_color_white(), 0);
}

void UI::firmware_popup_clicked_cb(lv_event_t *e) {
  UI *ui_instance = (UI *)lv_event_get_user_data(e);
  // The firmware issue notice can be acknowledged; the update notice stays until it ends
  if (ui_instance && ui_instance->firmware_popup_dismissable)
    lv_obj_add_flag(ui_instance->firmware_popup, LV_OBJ_FLAG_HIDDEN);
}

void UI::update_sensorstack_firmware_state(int state) {
  // state: 0 OK, 1 update in progress, 2 firmware issue (SensorStackFirmwareState)
  sensorstack_firmware_state = state;
  refresh_sensor_icon();
  if (tileManager)
    tileManager->update_sensorstack_firmware_state(state);

  if (state != 1 && state != 2) {
    if (firmware_popup)
      lv_obj_add_flag(firmware_popup, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (!firmware_popup)
    create_firmware_popup();

  // Return to the landing page underneath the popup
  if (tileManager &&
      lv_tileview_get_tile_act(tileview) != tileManager->get_tile1()) {
    lv_obj_set_tile_id(tileview, 0, 0, LV_ANIM_OFF);
    tileManager->update_tile_visibility(tileManager->get_tile1());
  }

  lv_obj_t *panel = lv_obj_get_parent(firmware_popup_icon);
  if (state == 1) {
    firmware_popup_dismissable = false;
    ui_set_label_text(firmware_popup_icon, ICON_UPDATE);
    ui_set_text_color(firmware_popup_icon, COLOR_HA_BLUE);
    lv_obj_set_style_border_color(panel, COLOR_HA_BLUE, 0);
    ui_set_label_text(firmware_popup_label,
                      "Updating\nSensorStack\nfirmware...");
  } else {
    firmware_popup_dismissable = true;
    ui_set_label_text(firmware_popup_icon, ICON_ALERT);
    ui_set_text_color(firmware_popup_icon, COLOR_HIGH);
    lv_obj_set_style_border_color(panel, COLOR_HIGH, 0);
    ui_set_label_text(firmware_popup_label,
                      "SensorStack\nfirmware issue\nUpload firmware\nfrom web page");
  }
  lv_obj_clear_flag(firmware_popup, LV_OBJ_FLAG_HIDDEN);
}

void UI::disp_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area,
                       lv_color_t *color_p) {
  if (_tft == nullptr)
    return;
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  // Wait for the previous DMA transfer (from the other buffer) to finish
  _tft->waitDMA();
  _tft->startWrite();
  _tft->setAddrWindow(area->x1, area->y1, w, h);
  _tft->pushPixelsDMA((lgfx::rgb565_t *)&color_p->full, w * h);
  _tft->endWrite();
  // Safe to signal now: LVGL renders into the other buffer while DMA drains
  // this one.
  lv_disp_flush_ready(disp);
}

void UI::touchpad_read_cb(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  if (_touch == nullptr)
    return;
  static uint16_t last_x = 0, last_y = 0;
  uint16_t touchX, touchY;
  uint8_t gesture;
  bool touched = _touch->getTouch(&touchX, &touchY, &gesture);
  if (touched) {
    uint16_t rotatedX = touchY;
    uint16_t rotatedY = (CST820::PANEL_WIDTH - 1) - touchX;
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
  UI *ui = static_cast<UI *>(timer->user_data);
  if (ui)
    ui->inactivity_timer_cb(timer);
}

void UI::inactivity_timer_cb(lv_timer_t *timer) {
  // Get the delay from ConfigManager - this is safe to call from UITask context
  // since ConfigManagerAccessor is thread-safe
  ConfigManagerAccessor config;
  uint32_t delay = config->getInactivityTimerDelay() *
                   1000; // Convert seconds to milliseconds

  if (lv_disp_get_inactive_time(NULL) > delay) {
    if (tileManager &&
        lv_tileview_get_tile_act(tileview) != tileManager->get_tile1()) {
      lv_obj_set_tile_id(tileview, 0, 0, LV_ANIM_ON);
      // Update tile visibility when returning to main tile
      tileManager->update_tile_visibility(tileManager->get_tile1());
    }
  }
}

void UI::tile_change_event_cb_static(lv_event_t *e) {
  UI *ui_instance = (UI *)lv_event_get_user_data(e);
  if (ui_instance)
    ui_instance->tile_change_event_cb(e);
}

void UI::tile_change_event_cb(lv_event_t *e) {
  lv_obj_t *active_tile = lv_tileview_get_tile_act(tileview);

  // Clear SGP41 test results when leaving SGP41 tile
  static lv_obj_t *prev_tile = nullptr;
  if (tileManager && prev_tile == tileManager->get_sgp41_tile() &&
      active_tile != prev_tile) {
    tileManager->clear_sgp41_results();
  }
  prev_tile = active_tile;

  // Update tile visibility based on adjacency
  if (tileManager) {
    tileManager->update_tile_visibility(active_tile);
  }

  if (tileManager && (active_tile == tileManager->get_tile1() ||
                      active_tile == tileManager->get_secondary_tile() ||
                      active_tile == tileManager->get_gas_tile() ||
                      active_tile == tileManager->get_powerdraw_tile())) {
    // Show status bar for first column tiles
    if (status_bar)
      lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_HIDDEN);
    // Force grid layout refresh to maintain proper alignment
    lv_obj_mark_layout_as_dirty(active_tile);
  } else {
    // Hide status bar for other columns
    if (status_bar)
      lv_obj_add_flag(status_bar, LV_OBJ_FLAG_HIDDEN);
  }
}
