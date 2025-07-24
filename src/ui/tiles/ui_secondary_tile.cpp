#include "ui/tiles/ui_secondary_tile.h"
#include "ui/custom_icons.h"

const int SCROLL_SPEED_MS_PX_SEC = 5;

UISecondaryTile::UISecondaryTile(ConfigManager* config) : _config(config) {}

lv_obj_t* UISecondaryTile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 0, 1, LV_DIR_TOP);
    lv_obj_set_scroll_dir(tile, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
    
    static lv_coord_t col_dsc[] = {24, LV_GRID_CONTENT, lv_grid_fr(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_set_layout(tile, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(tile, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(tile, row_dsc, 0);
    lv_obj_set_style_pad_all(tile, 5, 0);
    lv_obj_set_style_pad_row(tile, 12, 0);
    lv_obj_set_style_pad_column(tile, 10, 0);
    lv_obj_set_style_text_font(tile, &custom_font_30, 0);

    pm1_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(pm1_icon, &mdi_30, 0);
    lv_label_set_text(pm1_icon, ICON_BLUR);
    lv_obj_set_style_text_color(pm1_icon, COLOR_DEFAULT_ICON, 0);
    lv_obj_set_grid_cell(pm1_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    pm1_label = lv_label_create(tile);
    lv_label_set_text(pm1_label, "PM1.0: --- µg/m³");
    lv_obj_set_width(pm1_label, 228);
    lv_label_set_long_mode(pm1_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(pm1_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(pm1_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(pm1_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    pm25_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(pm25_icon, &mdi_30, 0);
    lv_label_set_text(pm25_icon, ICON_BLUR);
    lv_obj_set_style_text_color(pm25_icon, COLOR_DEFAULT_ICON, 0);
    lv_obj_set_grid_cell(pm25_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    pm25_label = lv_label_create(tile);
    lv_label_set_text(pm25_label, "PM2.5: --- µg/m³");
    lv_obj_set_width(pm25_label, 228);
    lv_label_set_long_mode(pm25_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(pm25_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(pm25_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(pm25_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    
    pm4_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(pm4_icon, &mdi_30, 0);
    lv_label_set_text(pm4_icon, ICON_BLUR);
    lv_obj_set_style_text_color(pm4_icon, COLOR_DEFAULT_ICON, 0);
    lv_obj_set_grid_cell(pm4_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    pm4_label = lv_label_create(tile);
    lv_label_set_text(pm4_label, "PM4.0: --- µg/m³");
    lv_obj_set_width(pm4_label, 228);
    lv_label_set_long_mode(pm4_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(pm4_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(pm4_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(pm4_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    
    pm10_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(pm10_icon, &mdi_30, 0);
    lv_label_set_text(pm10_icon, ICON_BLUR);
    lv_obj_set_style_text_color(pm10_icon, COLOR_DEFAULT_ICON, 0);
    lv_obj_set_grid_cell(pm10_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);

    pm10_label = lv_label_create(tile);
    lv_label_set_text(pm10_label, "PM10: --- µg/m³");
    lv_obj_set_width(pm10_label, 228);
    lv_label_set_long_mode(pm10_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(pm10_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(pm10_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(pm10_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 3, 1);

    fan_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(fan_icon, &mdi_30, 0);
    lv_label_set_text(fan_icon, ICON_FAN_OFF);
    lv_obj_set_style_text_color(fan_icon, COLOR_DEFAULT_ICON, 0);
    lv_obj_set_grid_cell(fan_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 4, 1);

    fan_label = lv_label_create(tile);
    lv_label_set_text(fan_label, "--- A");
    lv_obj_set_width(fan_label, 228);
    lv_label_set_long_mode(fan_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(fan_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(fan_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(fan_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    
    fan_alert_icons.main_screen_icon = fan_icon;
    
    return tile;
}

void UISecondaryTile::set_fan_status_icon(lv_obj_t* icon) {
    fan_alert_icons.status_bar_icon = icon;
}

void UISecondaryTile::clear_readings() {
    lv_label_set_text(pm1_label, "PM1.0: --- µg/m³");
    lv_label_set_text(pm25_label, "PM2.5: --- µg/m³");
    lv_label_set_text(pm4_label, "PM4.0: --- µg/m³");
    lv_label_set_text(pm10_label, "PM10: --- µg/m³");
    lv_label_set_text(fan_label, "--- A");

    if(pm1_icon) lv_obj_set_style_text_color(pm1_icon, COLOR_DEFAULT_ICON, 0);
    if(pm25_icon) lv_obj_set_style_text_color(pm25_icon, COLOR_DEFAULT_ICON, 0);
    if(pm4_icon) lv_obj_set_style_text_color(pm4_icon, COLOR_DEFAULT_ICON, 0);
    if(pm10_icon) lv_obj_set_style_text_color(pm10_icon, COLOR_DEFAULT_ICON, 0);

    lv_color_t default_text_color = lv_theme_get_color_primary(lv_obj_get_parent(pm1_label));
    if(pm1_label) lv_obj_set_style_text_color(pm1_label, default_text_color, 0);
    if(pm25_label) lv_obj_set_style_text_color(pm25_label, default_text_color, 0);
    if(pm4_label) lv_obj_set_style_text_color(pm4_label, default_text_color, 0);
    if(pm10_label) lv_obj_set_style_text_color(pm10_label, default_text_color, 0);

    lv_label_set_text(fan_icon, ICON_FAN_OFF);
    lv_obj_set_style_text_color(fan_icon, COLOR_DISCONNECTED, 0);
}

void UISecondaryTile::update_pm_values(float pm1, float pm25, float pm4, float pm10) {
    lv_label_set_text_fmt(pm1_label, "PM1.0: %.1f µg/m³", pm1);
    lv_label_set_text_fmt(pm25_label, "PM2.5: %.1f µg/m³", pm25);
    lv_label_set_text_fmt(pm4_label, "PM4.0: %.1f µg/m³", pm4);
    lv_label_set_text_fmt(pm10_label, "PM10:  %.1f µg/m³", pm10);

    lv_color_t pm1_color;
    if (pm1 < _config->getPm1WarnThreshold()) { pm1_color = COLOR_GREEN; }
    else if (pm1 < _config->getPm1DangerThreshold()) { pm1_color = COLOR_MID; }
    else { pm1_color = COLOR_HIGH; }
    lv_obj_set_style_text_color(pm1_label, pm1_color, 0);
    lv_obj_set_style_text_color(pm1_icon, pm1_color, 0);

    lv_color_t pm25_color;
    if (pm25 < _config->getPm25WarnThreshold()) { pm25_color = COLOR_GREEN; }
    else if (pm25 < _config->getPm25DangerThreshold()) { pm25_color = COLOR_MID; }
    else { pm25_color = COLOR_HIGH; }
    lv_obj_set_style_text_color(pm25_label, pm25_color, 0);
    lv_obj_set_style_text_color(pm25_icon, pm25_color, 0);

    lv_color_t pm4_color;
    if (pm4 < _config->getPm4WarnThreshold()) { pm4_color = COLOR_GREEN; }
    else if (pm4 < _config->getPm4DangerThreshold()) { pm4_color = COLOR_MID; }
    else { pm4_color = COLOR_HIGH; }
    lv_obj_set_style_text_color(pm4_label, pm4_color, 0);
    lv_obj_set_style_text_color(pm4_icon, pm4_color, 0);

    lv_color_t pm10_color;
    if (pm10 < _config->getPm10WarnThreshold()) { pm10_color = COLOR_GREEN; }
    else if (pm10 < _config->getPm10DangerThreshold()) { pm10_color = COLOR_MID; }
    else { pm10_color = COLOR_HIGH; }
    lv_obj_set_style_text_color(pm10_label, pm10_color, 0);
    lv_obj_set_style_text_color(pm10_icon, pm10_color, 0);
}

void UISecondaryTile::update_fan_current(float amps, FanStatus status) {
    lv_label_set_text_fmt(fan_label, "%.2f A", amps);
    
    switch (status) {
        case FAN_STATUS_OFF:
            lv_label_set_text(fan_icon, ICON_FAN_OFF);
            lv_obj_set_style_text_color(fan_label, COLOR_DEFAULT_ICON, 0);
            lv_obj_set_style_text_color(fan_icon, COLOR_DEFAULT_ICON, 0);
            break;
        case FAN_STATUS_NORMAL:
            lv_label_set_text(fan_icon, ICON_FAN);
            lv_obj_set_style_text_color(fan_label, COLOR_GREEN, 0);
            lv_obj_set_style_text_color(fan_icon, COLOR_GREEN, 0);
            break;
        case FAN_STATUS_ALERT:
            lv_label_set_text(fan_icon, ICON_FAN_ALERT);
            lv_obj_set_style_text_color(fan_label, COLOR_HIGH, 0);
            lv_obj_set_style_text_color(fan_icon, COLOR_HIGH, 0);
            break;
    }
}

void UISecondaryTile::update_fan_status(FanStatus status) {
    if (fan_blink_timer && !fan_blink_timer->paused) {
        lv_timer_pause(fan_blink_timer);
        lv_obj_set_style_opa(fan_alert_icons.status_bar_icon, LV_OPA_COVER, 0);
        lv_obj_set_style_opa(fan_alert_icons.main_screen_icon, LV_OPA_COVER, 0);
    }

    switch (status) {
        case FAN_STATUS_NORMAL:
            lv_obj_clear_flag(fan_alert_icons.status_bar_icon, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(fan_alert_icons.status_bar_icon, ICON_FAN);
            lv_obj_set_style_text_color(fan_alert_icons.status_bar_icon, COLOR_GREEN, 0);
            break;
        case FAN_STATUS_ALERT:
            lv_obj_clear_flag(fan_alert_icons.status_bar_icon, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(fan_alert_icons.status_bar_icon, ICON_FAN_ALERT);
            lv_obj_set_style_text_color(fan_alert_icons.status_bar_icon, COLOR_HIGH, 0);
            if(fan_blink_timer && fan_blink_timer->paused) {
                lv_timer_reset(fan_blink_timer);
                lv_timer_resume(fan_blink_timer);
            } else if (!fan_blink_timer) {
                fan_blink_timer = lv_timer_create(fan_alert_blink_cb, 750, &fan_alert_icons);
            }
            break;
        case FAN_STATUS_OFF:
            lv_obj_add_flag(fan_alert_icons.status_bar_icon, LV_OBJ_FLAG_HIDDEN);
            break;
     }
}

void UISecondaryTile::fan_alert_blink_cb(lv_timer_t* timer) {
    FanAlertIcons_t* icons = (FanAlertIcons_t*)timer->user_data;
    if (!icons || !icons->status_bar_icon || !icons->main_screen_icon) return;
    
    lv_opa_t current_opa = lv_obj_get_style_opa(icons->status_bar_icon, 0);
    lv_opa_t new_opa = (current_opa == LV_OPA_COVER) ? LV_OPA_TRANSP : LV_OPA_COVER;

    lv_obj_set_style_opa(icons->status_bar_icon, new_opa, 0);
    lv_obj_set_style_opa(icons->main_screen_icon, new_opa, 0);
}

void UISecondaryTile::update_pm1(float pm1) {
    lv_label_set_text_fmt(pm1_label, "PM1.0: %.1f µg/m³", pm1);
}
void UISecondaryTile::update_pm25(float pm25) {
    lv_label_set_text_fmt(pm25_label, "PM2.5: %.1f µg/m³", pm25);
}
void UISecondaryTile::update_pm4(float pm4) {
    lv_label_set_text_fmt(pm4_label, "PM4.0: %.1f µg/m³", pm4);
}
void UISecondaryTile::update_pm10(float pm10) {
    lv_label_set_text_fmt(pm10_label, "PM10:  %.1f µg/m³", pm10);
}
void UISecondaryTile::update_fan_amps(float amps) {
    lv_label_set_text_fmt(fan_label, "%.2f A", amps);
}