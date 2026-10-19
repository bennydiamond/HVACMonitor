#include "ui/tiles/ui_reboot_tile.h"
#include "SerialMutex.h"
#include "NanoCommands.h"
#include <Arduino.h>
#include "MainTaskEvents.h"

// Layout constants
const int TITLE_Y = 10;
const int REBOOT_NANO_BTN_Y = 50;
const int REBOOT_ESP32_BTN_Y = 110;
const int LABEL_X = 10;

UIRebootTile::UIRebootTile() : 
tile_obj(nullptr)
{}

UIRebootTile::~UIRebootTile() {

}

lv_obj_t* UIRebootTile::create_tile(lv_obj_t* parent_tv) {
    tile_obj = lv_tileview_add_tile(parent_tv, 1, 2, LV_DIR_HOR | LV_DIR_TOP);
    lv_obj_t* tile = tile_obj;
    
    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "Reboot");
    lv_obj_set_style_text_font(title, &custom_font_30, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, TITLE_Y);
    
    lv_obj_t* reboot_nano_btn = lv_btn_create(tile);
    lv_obj_align(reboot_nano_btn, LV_ALIGN_TOP_MID, 0, REBOOT_NANO_BTN_Y);
    lv_obj_add_event_cb(reboot_nano_btn, reboot_nano_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* nano_btn_label = lv_label_create(reboot_nano_btn);
    lv_label_set_text(nano_btn_label, "SensorStack");
    lv_obj_center(nano_btn_label);

    lv_obj_t* reboot_esp32_btn = lv_btn_create(tile);
    lv_obj_align(reboot_esp32_btn, LV_ALIGN_TOP_MID, 0, REBOOT_ESP32_BTN_Y);
    lv_obj_add_event_cb(reboot_esp32_btn, reboot_esp32_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* esp32_btn_label = lv_label_create(reboot_esp32_btn);
    lv_label_set_text(esp32_btn_label, "ESP32");
    lv_obj_center(esp32_btn_label);
    
    return tile;
}

void UIRebootTile::reboot_nano_btn_cb(lv_event_t* e) {
    MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_REBOOT_NANO);
}

void UIRebootTile::reboot_esp32_btn_cb(lv_event_t* e) {
    MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_REBOOT_ESP32);
}

void UIRebootTile::set_visible(bool visible) {
    if (tile_obj) {
        if (visible) {
            lv_obj_clear_flag(tile_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(tile_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}