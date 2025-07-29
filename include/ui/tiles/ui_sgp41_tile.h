#ifndef UI_SGP41_TILE_H
#define UI_SGP41_TILE_H

#include "../ui_common.h"

class UISGP41Tile {
public:
    lv_obj_t* create_tile(lv_obj_t* parent_tv);
    void update_test_result(int result, uint16_t value);
    void clear_test_results();
    void update_sgp41_test_status(int ret_status);
    void update_sgp41_test_value(uint16_t raw_value);
    static void selftest_btn_cb(lv_event_t* e);
private:
    lv_obj_t* result_label;
    lv_obj_t* value_label;
};

#endif // UI_SGP41_TILE_H