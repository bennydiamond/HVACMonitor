#ifndef UI_SGP40_TILE_H
#define UI_SGP40_TILE_H

#include "../ui_common.h"

class UISGP40Tile {
public:
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_test_result(int result, uint16_t value);
    void clear_test_results();
    static void selftest_btn_cb(lv_event_t* e);
private:
    lv_obj_t* result_label;
    lv_obj_t* value_label;
};

#endif // UI_SGP40_TILE_H