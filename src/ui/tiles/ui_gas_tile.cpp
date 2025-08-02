#include "ui/tiles/ui_gas_tile.h"
#include "ui/custom_icons.h"

const int SCROLL_SPEED_MS_PX_SEC = 5;

UIGasTile::UIGasTile() {}

lv_obj_t* UIGasTile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 0, 2, LV_DIR_VER);
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

    // NO2
    no2_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(no2_icon, &mdi_30, 0);
    lv_label_set_text(no2_icon, ICON_SMOKE); // Using CO2 molecule icon for NO2
    lv_obj_set_style_text_color(no2_icon, COLOR_DEFAULT_ICON, 0);
    lv_obj_set_grid_cell(no2_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    no2_label = lv_label_create(tile);
    lv_label_set_text(no2_label, "NO2: --- µg/m³");
    lv_obj_set_width(no2_label, 228);
    lv_label_set_long_mode(no2_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(no2_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(no2_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(no2_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    // O3
    o3_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(o3_icon, &mdi_30, 0);
    lv_label_set_text(o3_icon, ICON_MOLECULE); // Using CO2 molecule icon for O3
    lv_obj_set_style_text_color(o3_icon, COLOR_DEFAULT_ICON, 0);
    lv_obj_set_grid_cell(o3_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    o3_label = lv_label_create(tile);
    lv_label_set_text(o3_label, "O3: --- µg/m³");
    lv_obj_set_width(o3_label, 228);
    lv_label_set_long_mode(o3_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(o3_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(o3_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(o3_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    // NOx
    nox_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(nox_icon, &mdi_30, 0);
    lv_label_set_text(nox_icon, ICON_CAR_SIDE); // Using CO2 molecule icon for NOx
    lv_obj_set_style_text_color(nox_icon, COLOR_DEFAULT_ICON, 0);
    lv_obj_set_grid_cell(nox_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    nox_label = lv_label_create(tile);
    lv_label_set_text(nox_label, "NOx: ---");
    lv_obj_set_width(nox_label, 228);
    lv_label_set_long_mode(nox_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(nox_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(nox_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(nox_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    // CO (placeholder)
    co_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(co_icon, &mdi_30, 0);
    lv_label_set_text(co_icon, ICON_MOLECULE_CO); // Using CO2 molecule icon for CO
    lv_obj_set_style_text_color(co_icon, COLOR_DISCONNECTED, 0);
    lv_obj_set_grid_cell(co_icon, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);

    co_label = lv_label_create(tile);
    lv_label_set_text(co_label, "CO: --- ppm");
    lv_obj_set_width(co_label, 228);
    lv_label_set_long_mode(co_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(co_label, SCROLL_SPEED_MS_PX_SEC, 0);
    lv_obj_set_style_text_font(co_label, &custom_font_30, 0);
    lv_obj_set_grid_cell(co_label, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    
    return tile;
}

void UIGasTile::clear_readings() {
    lv_label_set_text(no2_label, "NO2: --- µg/m³");
    lv_label_set_text(o3_label, "O3:  --- µg/m³");
    lv_label_set_text(nox_label, "NOx: ---");
    lv_label_set_text(co_label, "CO:   --- ppm");

    lv_color_t default_text_color = lv_theme_get_color_primary(lv_obj_get_parent(no2_label));
    if(no2_label) lv_obj_set_style_text_color(no2_label, default_text_color, 0);
    if(o3_label) lv_obj_set_style_text_color(o3_label, default_text_color, 0);
    if(nox_label) lv_obj_set_style_text_color(nox_label, default_text_color, 0);
    if(co_label) lv_obj_set_style_text_color(co_label, default_text_color, 0);

    lv_obj_set_style_text_color(no2_icon, COLOR_DISCONNECTED, 0);
    lv_obj_set_style_text_color(o3_icon, COLOR_DISCONNECTED, 0);
    lv_obj_set_style_text_color(nox_icon, COLOR_DISCONNECTED, 0);
    lv_obj_set_style_text_color(co_icon, COLOR_DISCONNECTED, 0);
}

void UIGasTile::update_no2(float no2) {
    lv_label_set_text_fmt(no2_label, "NO2: %.0f µg/m³", no2);
    lv_color_t color;
    ConfigManagerAccessor config;
    if (no2 <= config->getNo2WarnThreshold()) { // Good air quality
        color = COLOR_GREEN;
    } else if (no2 <= config->getNo2DangerThreshold()) { // Moderate
        color = COLOR_MID;
    } else { // Poor air quality
        color = COLOR_HIGH;
    }
    lv_obj_set_style_text_color(no2_label, color, 0);
    lv_obj_set_style_text_color(no2_icon, color, 0);
}

void UIGasTile::update_o3(float o3) {
    lv_label_set_text_fmt(o3_label, "O3: %.0f µg/m³", o3);
    lv_color_t color;
    ConfigManagerAccessor config;
    if (o3 <= config->getO3WarnThreshold()) { // Good air quality
        color = COLOR_GREEN;
    } else if (o3 <= config->getO3DangerThreshold()) { // Moderate
        color = COLOR_MID;
    } else { // Poor air quality
        color = COLOR_HIGH;
    }
    lv_obj_set_style_text_color(o3_label, color, 0);
    lv_obj_set_style_text_color(o3_icon, color, 0);
}

void UIGasTile::update_nox(float nox) {
    lv_label_set_text_fmt(nox_label, "NOx: %.0f", nox);
    lv_color_t color;
    ConfigManagerAccessor config;
    if (nox <= config->getNoxWarnThreshold()) { // Excellent air quality
        color = COLOR_GREEN;
    } else if (nox <= config->getNoxDangerThreshold()) { // Good air quality
        color = COLOR_MID;
    } else { // Poor air quality
        color = COLOR_HIGH;
    }
    lv_obj_set_style_text_color(nox_label, color, 0);
    lv_obj_set_style_text_color(nox_icon, color, 0);
}

void UIGasTile::update_co(float co) {
    lv_label_set_text_fmt(co_label, "CO: %.1f ppm", co);
    lv_color_t color;
    ConfigManagerAccessor config;
    if (co <= config->getCoWarnThreshold()) { // Good air quality
        color = COLOR_GREEN;
    } else if (co <= config->getCoDangerThreshold()) { // Moderate
        color = COLOR_MID;
    } else { // Poor air quality
        color = COLOR_HIGH;
    }
    lv_obj_set_style_text_color(co_label, color, 0);
    lv_obj_set_style_text_color(co_icon, color, 0);
} 