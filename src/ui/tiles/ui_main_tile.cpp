#include "ui/tiles/ui_main_tile.h"
#include "ui/custom_icons.h"

UIMainTile::UIMainTile() {}

lv_obj_t* UIMainTile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 0, 0, LV_DIR_RIGHT | LV_DIR_VER);
    lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
    
    static lv_coord_t main_col_dsc[] = {24, LV_GRID_CONTENT, lv_grid_fr(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t main_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_set_layout(tile, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(tile, main_col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(tile, main_row_dsc, 0);
    lv_obj_set_style_pad_row(tile, 10, 0);
    lv_obj_set_style_pad_column(tile, 10, 0);

    create_pressure_widgets(tile);
    create_co2_widgets(tile);
    create_voc_widgets(tile);
    create_geiger_widgets(tile);
    create_temp_humi_widgets(tile);

    return tile;
}

void UIMainTile::create_pressure_widgets(lv_obj_t* parent) {
    pressure_icon = lv_label_create(parent);
    lv_obj_set_style_text_font(pressure_icon, &mdi_44, 0);
    lv_label_set_text(pressure_icon, ICON_GAUGE);
    lv_obj_set_grid_cell(pressure_icon, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_style_pad_bottom(pressure_icon, 18, 0);

    pressure_label = lv_label_create(parent);
    lv_label_set_text(pressure_label, "--- Pa");
    lv_obj_set_style_text_font(pressure_label, &custom_font_44, 0);
    lv_obj_set_grid_cell(pressure_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_style_pad_bottom(pressure_label, 18, 0);
}

void UIMainTile::create_co2_widgets(lv_obj_t* parent) {
    co2_icon = lv_label_create(parent);
    lv_obj_set_style_text_font(co2_icon, &mdi_30, 0);
    lv_label_set_text(co2_icon, ICON_MOLECULE_CO2);
    lv_obj_set_grid_cell(co2_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    co2_label = lv_label_create(parent);
    lv_label_set_text(co2_label, "---- ppm");
    lv_obj_set_style_text_font(co2_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(co2_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_style_pad_left(co2_label, -5, 0);
}

void UIMainTile::create_voc_widgets(lv_obj_t* parent) {
    voc_icon = lv_label_create(parent);
    lv_obj_set_style_text_font(voc_icon, &mdi_30, 0);
    lv_label_set_text(voc_icon, ICON_LUNGS);
    lv_obj_set_grid_cell(voc_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    voc_label = lv_label_create(parent);
    lv_label_set_text(voc_label, "VOC: ---");
    lv_obj_set_style_text_font(voc_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(voc_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_style_pad_left(voc_label, -5, 0);
}

void UIMainTile::create_geiger_widgets(lv_obj_t* parent) {
    usv_icon = lv_label_create(parent);
    lv_obj_set_style_text_font(usv_icon, &mdi_30, 0);
    lv_label_set_text(usv_icon, ICON_RADIOACTIVE);
    lv_obj_set_grid_cell(usv_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    
    usv_label = lv_label_create(parent);
    lv_label_set_text(usv_label, "-.-- µSv/h");
    lv_obj_set_style_text_font(usv_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(usv_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    lv_obj_set_style_pad_left(usv_label, -5, 0);
}

void UIMainTile::create_temp_humi_widgets(lv_obj_t* parent) {
    lv_obj_t* temp_humi_container = lv_obj_create(parent);
    lv_obj_remove_style_all(temp_humi_container);
    lv_obj_set_height(temp_humi_container, LV_SIZE_CONTENT);
    lv_obj_set_grid_cell(temp_humi_container, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_layout(temp_humi_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(temp_humi_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_humi_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(temp_humi_container, 0, 0);
    lv_obj_set_style_pad_gap(temp_humi_container, 0, 0);
    lv_obj_clear_flag(temp_humi_container, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_translate_x(temp_humi_container, -5, 0);
    
    temp_icon = lv_label_create(parent);
    lv_obj_set_style_text_font(temp_icon, &mdi_30, 0);
    lv_label_set_text(temp_icon, ICON_THERMOMETER);
    lv_obj_set_grid_cell(temp_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    
    temp_label = lv_label_create(temp_humi_container);
    lv_label_set_text(temp_label, "--.- °C");
    lv_obj_set_style_text_font(temp_label, &custom_font_30, 0);
    lv_obj_set_width(temp_label, 95); 
    lv_obj_set_style_text_align(temp_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(temp_label, LV_LABEL_LONG_CLIP);
    lv_obj_add_flag(temp_label, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t* humi_group = lv_obj_create(temp_humi_container);
    lv_obj_remove_style_all(humi_group);
    lv_obj_set_height(humi_group, LV_SIZE_CONTENT);
    lv_obj_set_layout(humi_group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(humi_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(humi_group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(humi_group, 0, 0);
    lv_obj_set_style_pad_gap(humi_group, 0, 0);
    lv_obj_set_style_pad_left(humi_group, -5, 0);

    humi_icon = lv_label_create(humi_group);
    lv_obj_set_style_text_font(humi_icon, &mdi_30, 0);
    lv_label_set_text(humi_icon, ICON_WATER_PERCENT);

    humi_label = lv_label_create(humi_group);
    lv_label_set_text(humi_label, "-- %");
    lv_obj_set_style_text_font(humi_label, &custom_font_30, 0);
}

void UIMainTile::clear_readings() {
    lv_label_set_text(pressure_label, "--- Pa");
    lv_label_set_text(usv_label, "-.-- µSv/h");
    lv_label_set_text(temp_label, "--.- °C");
    lv_label_set_text(humi_label, "-- %");
    lv_label_set_text(co2_label, "---- ppm");
    lv_label_set_text(voc_label, "VOC: ---");
}

void UIMainTile::update_pressure(float p) {
    char pressure_str[10];
    dtostrf(p, 4, 1, pressure_str);
    lv_label_set_text_fmt(pressure_label, "%s Pa", pressure_str);

    lv_color_t color;
    const char* icon_str;

    ConfigManagerAccessor config;
    if (p < config->getPressureLowThreshold()) {
        color = COLOR_GREEN;
        if (p < 30.0) { icon_str = ICON_GAUGE_EMPTY; } else { icon_str = ICON_GAUGE_LOW; }
    } else if (p < config->getPressureMidThreshold()) { 
        color = COLOR_MID;
        icon_str = ICON_GAUGE;
    } else { 
        color = COLOR_HIGH;
        icon_str = ICON_GAUGE_FULL;
    }
    
    lv_label_set_text(pressure_icon, icon_str);
    lv_obj_set_style_text_color(pressure_label, color, 0);
    lv_obj_set_style_text_color(pressure_icon, color, 0);
}

void UIMainTile::update_co2(float co2) {
    lv_label_set_text_fmt(co2_label, "%.0f ppm", co2);
    lv_color_t color;
    ConfigManagerAccessor config;
    if (co2 < config->getCo2WarnThreshold()) { color = COLOR_GREEN; }
    else if (co2 < config->getCo2DangerThreshold()) { color = COLOR_MID; }
    else { color = COLOR_HIGH; }
    lv_obj_set_style_text_color(co2_label, color, 0);
    lv_obj_set_style_text_color(co2_icon, color, 0);
}

void UIMainTile::update_voc(int32_t voc) {
    lv_label_set_text_fmt(voc_label, "VOC: %d", (int)voc);
    lv_color_t color;
    ConfigManagerAccessor config;
    if (voc < config->getVocWarnThreshold()) { color = COLOR_GREEN; }
    else if (voc < config->getVocDangerThreshold()) { color = COLOR_MID; }
    else { color = COLOR_HIGH; }
    lv_obj_set_style_text_color(voc_label, color, 0);
    lv_obj_set_style_text_color(voc_icon, color, 0);
}

void UIMainTile::update_geiger_cpm(int cpm) {
    // Optionally store cpm for use in update_geiger_usvh
    // For now, do nothing or update a label if you want
}
void UIMainTile::update_geiger_usvh(float usv) {
    char usv_str[10];
    dtostrf(usv, 4, 2, usv_str);
    lv_label_set_text_fmt(usv_label, "%s µSv/h", usv_str);
    
    lv_color_t color;
    ConfigManagerAccessor config;
    if (usv >= 0.00 && usv <= config->getGeigerAbnormalLowThreshold()) { 
        color = COLOR_HIGH;      // Abnormal low (red)
    } else if (usv > config->getGeigerAbnormalLowThreshold() && usv <= config->getGeigerAbnormalHighThreshold()) { 
        color = COLOR_GREEN;      // Normal (green)
    } else if (usv > config->getGeigerAbnormalHighThreshold() && usv <= config->getGeigerDangerHighThreshold()) { 
        color = COLOR_MID;        // Elevated (yellow)
    } else { 
        color = COLOR_HIGH;       // Danger high (red)
    }
    
    lv_obj_set_style_text_color(usv_label, color, 0);
    lv_obj_set_style_text_color(usv_icon, color, 0);
}
void UIMainTile::update_temp(float temp) {
    lv_label_set_text_fmt(temp_label, "%.1f °C", temp);
    
    lv_color_t color;
    ConfigManagerAccessor config;
    if (temp >= config->getTempComfortableLow() && temp <= config->getTempComfortableHigh()) { 
        color = COLOR_GREEN;     // Comfortable range
    } else if (temp >= config->getTempAcceptableLow() && temp <= config->getTempAcceptableHigh()) { 
        color = COLOR_MID;       // Acceptable range
    } else { 
        color = COLOR_HIGH;      // Extreme temperatures
    }
    
    lv_obj_set_style_text_color(temp_label, color, 0);
    lv_obj_set_style_text_color(temp_icon, color, 0);
}
void UIMainTile::update_humi(float humi) {
    lv_label_set_text_fmt(humi_label, "%.0f %%", humi);
    
    lv_color_t color;
    ConfigManagerAccessor config;
    if (humi >= config->getHumiComfortableLow() && humi <= config->getHumiComfortableHigh()) { 
        color = COLOR_GREEN;     // Comfortable range
    } else if (humi >= config->getHumiAcceptableLow() && humi <= config->getHumiAcceptableHigh()) { 
        color = COLOR_MID;       // Acceptable range
    } else { 
        color = COLOR_HIGH;      // Too dry or too humid
    }
    
    lv_obj_set_style_text_color(humi_label, color, 0);
    lv_obj_set_style_text_color(humi_icon, color, 0);
}