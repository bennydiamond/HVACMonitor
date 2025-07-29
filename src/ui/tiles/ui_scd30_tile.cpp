#include "ui/tiles/ui_scd30_tile.h"
#include "NanoCommands.h"
#include "SerialMutex.h"
#include <cstdio>
#include <Arduino.h>
#include "MainTaskEvents.h"

// Layout constants
const int TITLE_Y = 10;
const int AUTOCAL_LABEL_Y = 60;
const int AUTOCAL_SWITCH_Y = 55;
const int AUTOCAL_SWITCH_X = -10;
const int FORCECAL_LABEL_Y = 110;
const int LABEL_X = 10;
const int SWITCH_WIDTH = 80;
const int SWITCH_HEIGHT = 40;

extern void send_command_to_nano(char cmd);

lv_obj_t* UISCD30Tile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 2, 1, LV_DIR_HOR | LV_DIR_VER);
    
    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "SCD30");
    lv_obj_set_style_text_font(title, &custom_font_30, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, TITLE_Y);
    
    lv_obj_t* autocal_label = lv_label_create(tile);
    lv_label_set_text(autocal_label, "Auto Cal.");
    lv_obj_align(autocal_label, LV_ALIGN_TOP_LEFT, LABEL_X, AUTOCAL_LABEL_Y);
    
    autocal_switch = lv_switch_create(tile);
    lv_obj_set_size(autocal_switch, SWITCH_WIDTH, SWITCH_HEIGHT);
    lv_obj_align(autocal_switch, LV_ALIGN_TOP_RIGHT, AUTOCAL_SWITCH_X, AUTOCAL_SWITCH_Y);
    lv_obj_add_event_cb(autocal_switch, autocal_switch_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    
    forcecal_label = lv_label_create(tile);
    lv_label_set_text(forcecal_label, "Force Cal: 0 ppm");
    lv_obj_align(forcecal_label, LV_ALIGN_TOP_LEFT, LABEL_X, FORCECAL_LABEL_Y);
    
    return tile;
}

void UISCD30Tile::update_autocal_state(bool enabled) {
    if (enabled) {
        lv_obj_add_state(autocal_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(autocal_switch, LV_STATE_CHECKED);
    }
}

void UISCD30Tile::update_forcecal_value(uint16_t ppm) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Force Cal: %u ppm", ppm);
    lv_label_set_text(forcecal_label, buf);
}

void UISCD30Tile::autocal_switch_cb(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool state = lv_obj_has_state(sw, LV_STATE_CHECKED);
    MainTaskEventNotifier::getInstance().sendEvent(state ? MainTaskEventNotifier::EVT_SCD30_AUTOCAL_ON : MainTaskEventNotifier::EVT_SCD30_AUTOCAL_OFF);
}