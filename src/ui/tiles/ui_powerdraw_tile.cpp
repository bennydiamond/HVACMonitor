#include "ui/tiles/ui_powerdraw_tile.h"
#include "ui/custom_icons.h"
#include "ConfigManager.h"

const int SCROLL_SPEED_MS_PX_SEC = 5;

UIPowerdrawTile::UIPowerdrawTile() {}

lv_obj_t* UIPowerdrawTile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 0, 3, LV_DIR_VER);
    lv_obj_set_scroll_dir(tile, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
    
    static lv_coord_t col_dsc[] = {24, LV_GRID_CONTENT, lv_grid_fr(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_set_layout(tile, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(tile, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(tile, row_dsc, 0);
    lv_obj_set_style_pad_all(tile, 5, 0);
    lv_obj_set_style_pad_row(tile, 12, 0);
    lv_obj_set_style_pad_column(tile, 10, 0);
    lv_obj_set_style_text_font(tile, &custom_font_30, 0);

    // Fan Current
    fan_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(fan_icon, &mdi_30, 0);
    lv_label_set_text(fan_icon, ICON_FAN_OFF);
    lv_obj_set_style_text_color(fan_icon, COLOR_DISCONNECTED, 0);
    lv_obj_set_grid_cell(fan_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    fan_label = lv_label_create(tile);
    lv_label_set_text(fan_label, "Fan: --- A");
    lv_obj_set_width(fan_label, 228);
    lv_label_set_long_mode(fan_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(fan_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(fan_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(fan_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    // Compressor Current
    compressor_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(compressor_icon, &mdi_30, 0);
    lv_label_set_text(compressor_icon, ICON_ENGINE);
    lv_obj_set_style_text_color(compressor_icon, COLOR_DISCONNECTED, 0);
    lv_obj_set_grid_cell(compressor_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    compressor_label = lv_label_create(tile);
    lv_label_set_text(compressor_label, "Comp: --- A");
    lv_obj_set_width(compressor_label, 228);
    lv_label_set_long_mode(compressor_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(compressor_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(compressor_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(compressor_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    // Pump Current
    pump_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(pump_icon, &mdi_30, 0);
    lv_label_set_text(pump_icon, ICON_WATER_PUMP);
    lv_obj_set_style_text_color(pump_icon, COLOR_DISCONNECTED, 0);
    lv_obj_set_grid_cell(pump_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    pump_label = lv_label_create(tile);
    lv_label_set_text(pump_label, "Pompe: --- A");
    lv_obj_set_width(pump_label, 228);
    lv_label_set_long_mode(pump_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(pump_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(pump_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(pump_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    // Water Sensor
    water_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(water_icon, &mdi_30, 0);
    lv_label_set_text(water_icon, ICON_WATER);
    lv_obj_set_style_text_color(water_icon, COLOR_DISCONNECTED, 0);
    lv_obj_set_grid_cell(water_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);

    water_label = lv_label_create(tile);
    lv_label_set_text(water_label, "Panne: ---");
    lv_obj_set_width(water_label, 228);
    lv_label_set_long_mode(water_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(water_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(water_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(water_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    
    fan_alert_icons.main_screen_icon = fan_icon;
    
    return tile;
}

void UIPowerdrawTile::set_fan_status_icon(lv_obj_t* icon) {
    fan_alert_icons.status_bar_icon = icon;
}

void UIPowerdrawTile::clear_readings() {
    lv_label_set_text(fan_label, "Fan: --- A");
    lv_label_set_text(compressor_label, "Comp: --- A");
    lv_label_set_text(pump_label, "Pompe: --- A");
    lv_label_set_text(water_label, "Panne: ---");

    lv_color_t default_text_color = lv_theme_get_color_primary(lv_obj_get_parent(fan_label));
    if(fan_label) lv_obj_set_style_text_color(fan_label, default_text_color, 0);
    if(compressor_label) lv_obj_set_style_text_color(compressor_label, default_text_color, 0);
    if(pump_label) lv_obj_set_style_text_color(pump_label, default_text_color, 0);
    if(water_label) lv_obj_set_style_text_color(water_label, default_text_color, 0);

    lv_label_set_text(fan_icon, ICON_FAN_OFF);
    lv_obj_set_style_text_color(fan_icon, COLOR_DISCONNECTED, 0);
    lv_obj_set_style_text_color(compressor_icon, COLOR_DISCONNECTED, 0);
    lv_obj_set_style_text_color(pump_icon, COLOR_DISCONNECTED, 0);
    lv_label_set_text(water_icon, ICON_WATER);
    lv_obj_set_style_text_color(water_icon, COLOR_DISCONNECTED, 0);
}

void UIPowerdrawTile::update_fan_current(float amps, FanStatus status) {
    lv_label_set_text_fmt(fan_label, "Fan: %.2f A", amps);
    
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

void UIPowerdrawTile::update_compressor_current(float amps) {
    lv_label_set_text_fmt(compressor_label, "Comp: %.2f A", amps);
    
    lv_color_t color;
    ConfigManagerAccessor config;
    if (amps <= config->getCompressorOffCurrentThreshold()) {
        color = COLOR_DEFAULT_ICON;
        lv_obj_set_style_text_color(compressor_icon, COLOR_DEFAULT_ICON, 0);
    } else if (amps < config->getCompressorHighCurrentThreshold()) {
        color = COLOR_GREEN;
        lv_obj_set_style_text_color(compressor_icon, COLOR_GREEN, 0);
    } else { // High current alert
        color = COLOR_HIGH;
        lv_obj_set_style_text_color(compressor_icon, COLOR_HIGH, 0);
    }
    lv_obj_set_style_text_color(compressor_label, color, 0);
}

void UIPowerdrawTile::update_pump_current(float amps) {
    lv_label_set_text_fmt(pump_label, "Pompe: %.2f A", amps);
    
    lv_color_t color;
    ConfigManagerAccessor config;
    if (amps <= config->getPumpOffCurrentThreshold()) {
        color = COLOR_DEFAULT_ICON;
        lv_obj_set_style_text_color(pump_icon, COLOR_DEFAULT_ICON, 0);
    } else if (amps < config->getPumpHighCurrentThreshold()) {
        color = COLOR_GREEN;
        lv_obj_set_style_text_color(pump_icon, COLOR_GREEN, 0);
    } else { // High current alert
        color = COLOR_HIGH;
        lv_obj_set_style_text_color(pump_icon, COLOR_HIGH, 0);
    }
    lv_obj_set_style_text_color(pump_label, color, 0);
}

void UIPowerdrawTile::update_water_sensor(bool water_ok) {
    if (water_ok) {
        lv_label_set_text(water_label, "Panne: OK");
        lv_label_set_text(water_icon, ICON_WATER);
        lv_obj_set_style_text_color(water_label, COLOR_GREEN, 0);
        lv_obj_set_style_text_color(water_icon, COLOR_GREEN, 0);
    } else {
        lv_label_set_text(water_label, "Panne: Problème");
        lv_label_set_text(water_icon, ICON_WATER_ALERT);
        lv_obj_set_style_text_color(water_label, COLOR_HIGH, 0);
        lv_obj_set_style_text_color(water_icon, COLOR_HIGH, 0);
    }
}

void UIPowerdrawTile::update_fan_status(FanStatus status) {
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

void UIPowerdrawTile::fan_alert_blink_cb(lv_timer_t* timer) {
    FanAlertIcons_t* icons = (FanAlertIcons_t*)timer->user_data;
    if (!icons || !icons->status_bar_icon || !icons->main_screen_icon) return;
    
    lv_opa_t current_opa = lv_obj_get_style_opa(icons->status_bar_icon, 0);
    lv_opa_t new_opa = (current_opa == LV_OPA_COVER) ? LV_OPA_TRANSP : LV_OPA_COVER;

    lv_obj_set_style_opa(icons->status_bar_icon, new_opa, 0);
    lv_obj_set_style_opa(icons->main_screen_icon, new_opa, 0);
} 