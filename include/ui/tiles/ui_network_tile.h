#ifndef UI_NETWORK_TILE_H
#define UI_NETWORK_TILE_H

#include "../ui_common.h"

class UINetworkTile {
public:
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_network_info(const char* ip, const char* mac, int8_t rssi, const char* ssid, bool ha_conn);
private:
    lv_obj_t* ssid_label;
    lv_obj_t* rssi_label;
    lv_obj_t* ip_label;
    lv_obj_t* mac_label;
    lv_obj_t* ha_label;
};

#endif // UI_NETWORK_TILE_H