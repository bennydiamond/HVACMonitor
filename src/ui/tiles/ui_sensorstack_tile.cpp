#include "ui/tiles/ui_sensorstack_tile.h"
#include "ui/custom_icons.h"

extern const lv_font_t custom_font_30;
const int SCROLL_SPEED_MS_PX_SS = 5;

lv_obj_t* UISensorStackTile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 2, 0, LV_DIR_HOR | LV_DIR_VER);
    lv_obj_set_scroll_dir(tile, LV_DIR_NONE);
    
    static lv_coord_t col_dsc[] = {LV_GRID_CONTENT, lv_grid_fr(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_set_layout(tile, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(tile, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(tile, row_dsc, 0);
    lv_obj_set_style_pad_all(tile, 5, 0);
    lv_obj_set_style_pad_row(tile, 14, 0);
    lv_obj_set_style_pad_column(tile, 10, 0);
    lv_obj_set_style_text_font(tile, &custom_font_30, 0);

    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "SensorStack");
    lv_obj_set_style_text_font(title, &custom_font_30, 0);
    lv_obj_set_style_text_decor(title, LV_TEXT_DECOR_UNDERLINE, 0);
    lv_obj_set_grid_cell(title, LV_GRID_ALIGN_CENTER, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t* version_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(version_icon, &mdi_30, 0);
    lv_label_set_text(version_icon, ICON_INFO);
    lv_obj_set_grid_cell(version_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    version_label = lv_label_create(tile);
    lv_label_set_text(version_label, "FW: N/A");
    lv_obj_set_width(version_label, 270);
    lv_label_set_long_mode(version_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(version_label, SCROLL_SPEED_MS_PX_SS, 0);
    lv_obj_set_grid_cell(version_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    lv_obj_t* ram_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(ram_icon, &mdi_30, 0);
    lv_label_set_text(ram_icon, ICON_MEMORY);
    lv_obj_set_grid_cell(ram_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    ram_label = lv_label_create(tile);
    lv_label_set_text(ram_label, "RAM: N/A");
    lv_obj_set_width(ram_label, 270);
    lv_label_set_long_mode(ram_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(ram_label, SCROLL_SPEED_MS_PX_SS, 0);
    lv_obj_set_grid_cell(ram_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    lv_obj_t* uptime_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(uptime_icon, &mdi_30, 0);
    lv_label_set_text(uptime_icon, ICON_CLOCK);
    lv_obj_set_grid_cell(uptime_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    uptime_label = lv_label_create(tile);
    lv_label_set_text(uptime_label, "Up: N/A");
    lv_obj_set_width(uptime_label, 270);
    lv_label_set_long_mode(uptime_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(uptime_label, SCROLL_SPEED_MS_PX_SS, 0);
    lv_obj_set_grid_cell(uptime_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);

    status_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(status_icon, &mdi_30, 0);
    lv_label_set_text(status_icon, ICON_LAN_DISCONNECT);
    lv_obj_set_grid_cell(status_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_style_text_color(status_icon, COLOR_DISCONNECTED, 0);
    status_label = lv_label_create(tile);
    lv_label_set_text(status_label, "Disconnected");
    lv_obj_set_width(status_label, 270);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(status_label, SCROLL_SPEED_MS_PX_SS, 0);
    lv_obj_set_grid_cell(status_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_style_text_color(status_label, COLOR_DISCONNECTED, 0);
    
    return tile;
}

void UISensorStackTile::update_sensorstack_uptime(uint32_t uptime) {
    if(uptime_label) {
        unsigned long minutes = uptime / 60;
        unsigned long hours = minutes / 60;
        unsigned long days = hours / 24;
        lv_label_set_text_fmt(uptime_label, "Up: %lud %luh %02lum", days, hours % 24, minutes % 60);
    }
}
void UISensorStackTile::update_sensorstack_ram(uint16_t ram) {
    if(ram_label) {
        lv_label_set_text_fmt(ram_label, "RAM: %u B", ram);
    }
}
void UISensorStackTile::update_sensor_status(bool connected) {
    lv_label_set_text(status_icon, connected ? ICON_LAN_CONNECT : ICON_LAN_DISCONNECT);
    lv_label_set_text(status_label, connected ? "Connected" : "Disconnected");
    lv_obj_set_style_text_color(status_icon, connected ? COLOR_GREEN : COLOR_DISCONNECTED, 0);
    lv_obj_set_style_text_color(status_label, connected ? COLOR_GREEN : COLOR_DISCONNECTED, 0);
    if(connected == false) {
        update_sensorstack_uptime(0);
    }
}

void UISensorStackTile::update_fw_version(const char* fw) {
    if (version_label) lv_label_set_text(version_label, fw);
}