#ifndef UI_REBOOT_TILE_H
#define UI_REBOOT_TILE_H

#include "../ui_common.h"

class UIRebootTile {
public:
    UIRebootTile();
    ~UIRebootTile();
    
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void set_visible(bool visible);
    static void reboot_nano_btn_cb(lv_event_t* e);
    static void reboot_esp32_btn_cb(lv_event_t* e);
private:
    lv_obj_t* tile_obj;
};

#endif // UI_REBOOT_TILE_H