#include "ui/tiles/ui_sps30_tile.h"
#include "NanoCommands.h"
#include "SerialMutex.h"
#include <cstdio>
#include <Arduino.h>
#include "MainTaskEvents.h"

// Layout constants
const int TITLE_Y = 10;
const int INTERVAL_LABEL_Y = 60;
const int DAYS_LABEL_Y = 90;
const int CLEAN_BTN_Y = 130;
const int LABEL_X = 10;

lv_obj_t* UISPS30Tile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 2, 2, LV_DIR_RIGHT | LV_DIR_VER);
    
    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "SPS30");
    lv_obj_set_style_text_font(title, &custom_font_30, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, TITLE_Y);
    
    interval_label = lv_label_create(tile);
    lv_label_set_text(interval_label, "Fan sec.: N/A");
    lv_obj_align(interval_label, LV_ALIGN_TOP_LEFT, LABEL_X, INTERVAL_LABEL_Y);
    
    days_label = lv_label_create(tile);
    lv_label_set_text(days_label, "Fan Days: N/A");
    lv_obj_align(days_label, LV_ALIGN_TOP_LEFT, LABEL_X, DAYS_LABEL_Y);
    
    lv_obj_t* clean_btn = lv_btn_create(tile);
    lv_obj_align(clean_btn, LV_ALIGN_TOP_MID, 0, CLEAN_BTN_Y);
    lv_obj_add_event_cb(clean_btn, manual_clean_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* btn_label = lv_label_create(clean_btn);
    lv_label_set_text(btn_label, "Manual Clean");
    lv_obj_center(btn_label);
    

    
    return tile;
}

void UISPS30Tile::update_sps30_info(uint32_t fan_interval, uint8_t fan_days) {
    char buf[64];
    
    snprintf(buf, sizeof(buf), "Fan sec.: %lus", fan_interval);
    lv_label_set_text(interval_label, buf);
    
    snprintf(buf, sizeof(buf), "Fan Days: %u", fan_days);
    lv_label_set_text(days_label, buf);
}

void UISPS30Tile::update_sps30_fan_interval(unsigned long fan_interval) {
    if (interval_label) lv_label_set_text_fmt(interval_label, "Fan sec.: %lu", fan_interval);
}
void UISPS30Tile::update_sps30_fan_days(unsigned long fan_days) {
    if (days_label) lv_label_set_text_fmt(days_label, "Fan Days: %lu", fan_days);
}

void UISPS30Tile::manual_clean_cb(lv_event_t* e) {
    MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_SPS30_CLEAN);
}