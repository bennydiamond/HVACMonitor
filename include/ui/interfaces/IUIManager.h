#ifndef IUI_MANAGER_H
#define IUI_MANAGER_H

#include <lvgl.h> // For lv_obj_t if needed in future, though not directly for these methods

// Forward declarations for types used in method signatures
class LGFX;
class CST820;
class ConfigManager;

class IUIManager {
public:
    virtual ~IUIManager() = default;

    virtual void init(LGFX* tft, CST820* touch, ConfigManager* config) = 0;
    virtual void create_widgets(void) = 0;
    virtual void run(void) = 0; // For lv_timer_handler
};

#endif // IUI_MANAGER_H