#include "ui/tiles/ui_runtime_tile.h"
#include "ui/custom_icons.h"

const int SCROLL_SPEED_MS_PX_RT = 5;

lv_obj_t* UIRuntimeTile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 1, 0, LV_DIR_LEFT | LV_DIR_VER);
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

    lv_obj_t* version_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(version_icon, &mdi_30, 0);
    lv_label_set_text(version_icon, ICON_INFO);
    lv_obj_set_grid_cell(version_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    fw_label = lv_label_create(tile);
    lv_label_set_text(fw_label, "FW: ---");
    lv_obj_set_width(fw_label, 270);
    lv_label_set_long_mode(fw_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(fw_label, SCROLL_SPEED_MS_PX_RT, 0);
    lv_obj_set_grid_cell(fw_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t* mem_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(mem_icon, &mdi_30, 0);
    lv_label_set_text(mem_icon, ICON_MEMORY);
    lv_obj_set_grid_cell(mem_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    mem_label = lv_label_create(tile);
    lv_label_set_text(mem_label, "Mem: --- kB");
    lv_obj_set_width(mem_label, 270);
    lv_label_set_long_mode(mem_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(mem_label, SCROLL_SPEED_MS_PX_RT, 0);
    lv_obj_set_grid_cell(mem_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    lv_obj_t* uptime_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(uptime_icon, &mdi_30, 0);
    lv_label_set_text(uptime_icon, ICON_CLOCK);
    lv_obj_set_grid_cell(uptime_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    uptime_label = lv_label_create(tile);
    lv_label_set_text(uptime_label, "Up: ---");
    lv_obj_set_width(uptime_label, 270);
    lv_label_set_long_mode(uptime_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(uptime_label, SCROLL_SPEED_MS_PX_RT, 0);
    lv_obj_set_grid_cell(uptime_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    lv_obj_t* reset_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(reset_icon, &mdi_30, 0);
    lv_label_set_text(reset_icon, ICON_RESTART);
    lv_obj_set_grid_cell(reset_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    reset_label = lv_label_create(tile);
    lv_label_set_text(reset_label, "Reset: ---");
    lv_obj_set_width(reset_label, 270);
    lv_label_set_long_mode(reset_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(reset_label, SCROLL_SPEED_MS_PX_RT, 0);
    lv_obj_set_grid_cell(reset_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    
    pkt_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(pkt_icon, &mdi_30, 0);
    lv_label_set_text(pkt_icon, ICON_LAN_DISCONNECT);
    lv_obj_set_grid_cell(pkt_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);

    pkt_label = lv_label_create(tile);
    lv_label_set_text(pkt_label, "Pkt: --- s");
    lv_obj_set_width(pkt_label, 270);
    lv_label_set_long_mode(pkt_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(pkt_label, SCROLL_SPEED_MS_PX_RT, 0);
    lv_obj_set_grid_cell(pkt_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    return tile;
}

void UIRuntimeTile::set_initial_info(const char* ver, const char* rst_reason) {
    if(fw_label) lv_label_set_text_fmt(fw_label, "FW: %s", ver);
    if(reset_label) lv_label_set_text_fmt(reset_label, "Reset: %s", rst_reason);
}

void UIRuntimeTile::update_runtime_info(uint32_t freemem, unsigned long uptime) {
    if(mem_label) {
        if (freemem < 10000) {
            lv_label_set_text_fmt(mem_label, "Mem: %lu b", freemem);
        } else {
            lv_label_set_text_fmt(mem_label, "Mem: %lu kB", freemem / 1024);
        }
        lv_color_t color;
        if (freemem < 20480) { color = COLOR_HIGH; }
        else if (freemem < 51200) { color = COLOR_MID; }
        else { color = COLOR_GREEN; }
        lv_obj_set_style_text_color(mem_label, color, 0);
        lv_obj_t* mem_icon = lv_obj_get_child(lv_obj_get_parent(mem_label), lv_obj_get_index(mem_label) - 1);
        if (mem_icon) lv_obj_set_style_text_color(mem_icon, color, 0);
    }
    if(uptime_label) {
        unsigned long seconds = uptime / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;
        unsigned long days = hours / 24;
        lv_label_set_text_fmt(uptime_label, "Up: %lud %luh %02lum", days, hours % 24, minutes % 60);
    }
}

void UIRuntimeTile::update_last_packet_time(uint32_t secs, bool connected) {
    if (pkt_label) {
        lv_label_set_text_fmt(pkt_label, "Pkt: %lu s", secs);
        lv_color_t color = connected ? COLOR_GREEN : COLOR_DISCONNECTED;
        const char* icon = connected ? ICON_LAN_CONNECT : ICON_LAN_DISCONNECT;
        lv_obj_set_style_text_color(pkt_label, color, 0);
        if (pkt_icon) {
            lv_label_set_text(pkt_icon, icon);
            lv_obj_set_style_text_color(pkt_icon, color, 0);
        }
    }
}

void UIRuntimeTile::high_pressure_blink_cb(lv_timer_t* timer) {
    lv_obj_t* icon = (lv_obj_t*)timer->user_data;
    lv_opa_t current_opa = lv_obj_get_style_opa(icon, 0);
    lv_obj_set_style_opa(icon, (current_opa == LV_OPA_COVER) ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
}