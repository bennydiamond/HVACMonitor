#ifndef _CST820_H
#define _CST820_H

#include <Wire.h>

#define I2C_ADDR_CST820 0x15

// Gestures
enum GESTURE
{
    None = 0x00,       // No gesture
    SlideDown = 0x01,  // Slide down
    SlideUp = 0x02,    // Slide up
    SlideLeft = 0x03,  // Slide left
    SlideRight = 0x04, // Slide right
    SingleTap = 0x05,  // Single-tap
    DoubleTap = 0x0B,  // Double-tap
    LongPress = 0x0C   // Long-press
};

class CST820
{
public:
    CST820(int8_t sda_pin = -1, int8_t scl_pin = -1, int8_t rst_pin = -1, int8_t int_pin = -1);

    void begin(void);
    bool getTouch(uint16_t *x, uint16_t *y, uint8_t *gesture);

private:
    int8_t _sda, _scl, _rst, _int;

    uint8_t i2c_read(uint8_t addr);
    uint8_t i2c_read_continuous(uint8_t addr, uint8_t *data, uint32_t length);
    void i2c_write(uint8_t addr, uint8_t data);
    uint8_t i2c_write_continuous(uint8_t addr, const uint8_t *data, uint32_t length);
};
#endif