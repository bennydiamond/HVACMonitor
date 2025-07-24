#include "ui/tiles/ui_sgp41_tile.h"
#include "SerialMutex.h"
#include "NanoCommands.h"
#include <Arduino.h>
#include "MainTaskEvents.h"

// Layout constants
const int TITLE_Y = 10;
const int TEST_BTN_Y = 60;
const int RESULT_LABEL_Y = 120;
const int VALUE_LABEL_Y = 152;
const int LABEL_X = 10;

lv_obj_t* UISGP41Tile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 2, 3, LV_DIR_RIGHT | LV_DIR_VER);
    
    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "SGP41");
    lv_obj_set_style_text_font(title, &custom_font_30, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, TITLE_Y);
    
    lv_obj_t* test_btn = lv_btn_create(tile);
    lv_obj_align(test_btn, LV_ALIGN_TOP_MID, 0, TEST_BTN_Y);
    lv_obj_add_event_cb(test_btn, selftest_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* btn_label = lv_label_create(test_btn);
    lv_label_set_text(btn_label, "Self Test");
    lv_obj_center(btn_label);
    
    result_label = lv_label_create(tile);
    lv_label_set_text(result_label, "");
    lv_obj_align(result_label, LV_ALIGN_TOP_MID, LABEL_X, RESULT_LABEL_Y);
    
    value_label = lv_label_create(tile);
    lv_label_set_text(value_label, "");
    lv_obj_align(value_label, LV_ALIGN_TOP_MID, LABEL_X, VALUE_LABEL_Y);
    
    return tile;
}

void UISGP41Tile::update_test_result(int result, uint16_t value) {
    if (result != 0) {
        lv_label_set_text(result_label, "Test Result: Error!");
        lv_label_set_text(value_label, "");
        lv_obj_set_style_text_color(result_label, COLOR_HIGH, 0);
    } else if (value == 0xD400) {
        lv_label_set_text(result_label, "Test Result: OK!");
        lv_label_set_text_fmt(value_label, "Value: 0x%04X", value);
        lv_obj_set_style_text_color(result_label, COLOR_GREEN, 0);
        lv_obj_set_style_text_color(value_label, COLOR_GREEN, 0);
    } else {
        lv_label_set_text(result_label, "Test Result: Failed!");
        lv_label_set_text_fmt(value_label, "Value: 0x%04X", value);
        lv_obj_set_style_text_color(result_label, COLOR_MID, 0);
        lv_obj_set_style_text_color(value_label, COLOR_MID, 0);
    }
}

void UISGP41Tile::clear_test_results() {
    lv_label_set_text(result_label, "");
    lv_label_set_text(value_label, "");
}

void UISGP41Tile::update_sgp41_test_status(int ret_status) {
    if (result_label) {
        if (ret_status != 0) {
            lv_label_set_text(result_label, "Test Result: Error!");
            lv_obj_set_style_text_color(result_label, COLOR_HIGH, 0);
        } else {
            lv_label_set_text(result_label, "Test Result: OK!");
            lv_obj_set_style_text_color(result_label, COLOR_GREEN, 0);
        }
    }
}
void UISGP41Tile::update_sgp41_test_value(uint16_t raw_value) {
    if (value_label) {
        lv_label_set_text_fmt(value_label, "Value: 0x%04X", raw_value);
        lv_obj_set_style_text_color(value_label, COLOR_GREEN, 0);
    }
}

void UISGP41Tile::selftest_btn_cb(lv_event_t* e) {
    MainTaskEventNotifier::getInstance().sendEvent(MainTaskEventNotifier::EVT_SGP41_TEST);
}