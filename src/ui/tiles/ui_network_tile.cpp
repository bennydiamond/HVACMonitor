#include "ui/tiles/ui_network_tile.h"
#include "ui/custom_icons.h"

const int SCROLL_SPEED_MS_PX_NET = 5;

lv_obj_t* UINetworkTile::create_tile(lv_obj_t* parent_tv) {
    lv_obj_t* tile = lv_tileview_add_tile(parent_tv, 1, 1, LV_DIR_HOR | LV_DIR_TOP);
    lv_obj_set_scroll_dir(tile, LV_DIR_NONE);
    
    static lv_coord_t col_dsc[] = {LV_GRID_CONTENT, lv_grid_fr(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

    lv_obj_set_layout(tile, LV_LAYOUT_GRID);
    lv_obj_set_style_grid_column_dsc_array(tile, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(tile, row_dsc, 0);
    lv_obj_set_style_pad_all(tile, 5, 0);
    lv_obj_set_style_pad_row(tile, 12, 0);
    lv_obj_set_style_pad_column(tile, 10, 0);
    lv_obj_set_style_text_font(tile, &custom_font_30, 0);

    lv_obj_t* ssid_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(ssid_icon, &mdi_30, 0);
    lv_label_set_text(ssid_icon, ICON_WIFI);
    lv_obj_set_grid_cell(ssid_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    ssid_label = lv_label_create(tile);
    lv_label_set_text(ssid_label, "SSID: ---");
    lv_obj_set_width(ssid_label, 270);
    lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(ssid_label, SCROLL_SPEED_MS_PX_NET, 0);
    lv_obj_set_grid_cell(ssid_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);

    lv_obj_t* rssi_icon_obj = lv_label_create(tile);
    lv_obj_set_style_text_font(rssi_icon_obj, &mdi_30, 0);
    lv_label_set_text(rssi_icon_obj, ICON_WIFI_STRENGTH_4);
    lv_obj_set_grid_cell(rssi_icon_obj, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    rssi_label = lv_label_create(tile);
    lv_label_set_text(rssi_label, "RSSI: -- dBm");
    lv_obj_set_width(rssi_label, 270);
    lv_label_set_long_mode(rssi_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(rssi_label, SCROLL_SPEED_MS_PX_NET, 0);
    lv_obj_set_grid_cell(rssi_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

    lv_obj_t* ip_icon_obj = lv_label_create(tile);
    lv_obj_set_style_text_font(ip_icon_obj, &mdi_30, 0);
    lv_label_set_text(ip_icon_obj, ICON_IP);
    lv_obj_set_grid_cell(ip_icon_obj, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    ip_label = lv_label_create(tile);
    lv_label_set_text(ip_label, "IP: ---");
    lv_obj_set_width(ip_label, 270);
    lv_label_set_long_mode(ip_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(ip_label, SCROLL_SPEED_MS_PX_NET, 0);
    lv_obj_set_grid_cell(ip_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);

    lv_obj_t* mac_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(mac_icon, &mdi_30, 0);
    lv_label_set_text(mac_icon, ICON_DATABASE);
    lv_obj_set_grid_cell(mac_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    mac_label = lv_label_create(tile);
    lv_label_set_text(mac_label, "MAC: ---");
    lv_obj_set_width(mac_label, 260);
    lv_label_set_long_mode(mac_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(mac_label, SCROLL_SPEED_MS_PX_NET, 0);
    lv_obj_set_grid_cell(mac_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);

    lv_obj_t* ha_stat_icon = lv_label_create(tile);
    lv_obj_set_style_text_font(ha_stat_icon, &mdi_30, 0);
    lv_label_set_text(ha_stat_icon, ICON_HOME_ASSISTANT);
    lv_obj_set_grid_cell(ha_stat_icon, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    ha_label = lv_label_create(tile);
    lv_label_set_text(ha_label, "HA: ---");
    lv_obj_set_width(ha_label, 270);
    lv_label_set_long_mode(ha_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(ha_label, SCROLL_SPEED_MS_PX_NET, 0);
    lv_obj_set_grid_cell(ha_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 4, 1);

    return tile;
}

void UINetworkTile::update_network_rssi(int8_t rssi) {
    if(rssi_label) {
        lv_label_set_text_fmt(rssi_label, "RSSI: %d dBm", rssi);
    }
}
void UINetworkTile::update_network_ha_conn(bool ha_conn) {
    if(ha_label) {
        lv_color_t color = ha_conn ? COLOR_HA_BLUE : COLOR_DISCONNECTED;
        lv_obj_set_style_text_color(ha_label, color, 0);
        lv_obj_t* ha_icon_in_tile = lv_obj_get_child(lv_obj_get_parent(ha_label), lv_obj_get_index(ha_label) - 1);
        lv_obj_set_style_text_color(ha_icon_in_tile, color, 0);
        lv_label_set_text(ha_label, ha_conn ? "HA: Connected" : "HA: Disconnected");
    }
}

void UINetworkTile::update_ssid(const char* ssid) {
    if (ssid_label) lv_label_set_text(ssid_label, ssid);
}
void UINetworkTile::update_ip(const char* ip) {
    if (ip_label) lv_label_set_text(ip_label, ip);
}
void UINetworkTile::update_mac(const char* mac) {
    if (mac_label) lv_label_set_text(mac_label, mac);
}
