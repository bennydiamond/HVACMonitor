#ifndef UI_RUNTIME_TILE_H
#define UI_RUNTIME_TILE_H

#include "../ui_common.h"

class UIRuntimeTile {
public:
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void set_initial_info(const char* ver, const char* rst_reason);
    void update_runtime_info(uint32_t freemem, unsigned long uptime);
    void update_last_packet_time(uint32_t secs, bool connected);
    static void high_pressure_blink_cb(lv_timer_t* timer);
private:
    lv_obj_t* fw_label;
    lv_obj_t* mem_label;
    lv_obj_t* uptime_label;
    lv_obj_t* reset_label;
    lv_obj_t* pkt_icon, *pkt_label;
};

#endif // UI_RUNTIME_TILE_H