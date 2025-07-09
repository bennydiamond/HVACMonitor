#include "ui/tiles/ui_sgp40_tile.h"
#include "NanoCommands.h"
#include <Arduino.h>

// Layout constants
const int TITLE_Y = 10;
const int TEST_BTN_Y = 60;
const int RESULT_LABEL_Y = 120;
const int VALUE_LABEL_Y = 152;
const int LABEL_X = 10;

lv_obj_t* UISGP40Tile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 2, 3, LV_DIR_RIGHT | LV_DIR_VER);
    
    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "SGP40");
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

void UISGP40Tile::update_test_result(int result, uint16_t value) {
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

void UISGP40Tile::clear_test_results() {
    lv_label_set_text(result_label, "");
    lv_label_set_text(value_label, "");
}

void UISGP40Tile::selftest_btn_cb(lv_event_t* e) {
    uint8_t crc = 0x00;
    uint8_t polynomial = 0x07;
    char cmd = CMD_SGP40_TEST;
    
    crc ^= cmd;
    for (int j = 0; j < 8; j++) {
        crc = (crc & 0x80) ? (crc << 1) ^ polynomial : (crc << 1);
    }
    
    Serial.print('<'); Serial.print(cmd); Serial.print(','); Serial.print(crc); Serial.print('>');
}