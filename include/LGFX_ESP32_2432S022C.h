#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
    // Add an instance for the backlight controller
    lgfx::Light_PWM     _light_instance;

    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Panel_ST7789  _panel_instance;

public:
    LGFX(void)
    {
        { // Configure bus control
            auto bus_cfg = _bus_instance.config();
            bus_cfg.freq_write = 25000000;
            bus_cfg.pin_wr = 4;
            bus_cfg.pin_rd = 2;
            bus_cfg.pin_rs = 16;
            bus_cfg.pin_d0 = 15;
            bus_cfg.pin_d1 = 13;
            bus_cfg.pin_d2 = 12;
            bus_cfg.pin_d3 = 14;
            bus_cfg.pin_d4 = 27;
            bus_cfg.pin_d5 = 25;
            bus_cfg.pin_d6 = 33;
            bus_cfg.pin_d7 = 32;
            _bus_instance.config(bus_cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        { // Configure display panel control
            auto panel_cfg = _panel_instance.config();
            panel_cfg.pin_cs = 17;
            panel_cfg.pin_rst = -1;
            panel_cfg.pin_busy = -1;
            
            panel_cfg.panel_width = 240;
            panel_cfg.panel_height = 320;
            panel_cfg.offset_x = 0;
            panel_cfg.offset_y = 0;
            panel_cfg.offset_rotation = 0;
            panel_cfg.readable = false;
            
            // --- Revert to baseline color settings ---
            panel_cfg.invert = false;
            panel_cfg.rgb_order = false;
            // -----------------------------------------
            
            panel_cfg.dlen_16bit = false;
            panel_cfg.bus_shared = true;

            _panel_instance.config(panel_cfg);
        }

        // Configure Backlight
        { 
            auto cfg = _light_instance.config();
            cfg.pin_bl = 0;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        
        setPanel(&_panel_instance);
    }
};
