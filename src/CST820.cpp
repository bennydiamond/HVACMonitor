#include "CST820.h"
#include <Arduino.h>

CST820::CST820(int8_t sda_pin, int8_t scl_pin, int8_t rst_pin, int8_t int_pin)
{
    _sda = sda_pin;
    _scl = scl_pin;
    _rst = rst_pin;
    _int = int_pin;
}

void CST820::begin(void)
{
    Wire.begin(_sda, _scl);

    // Perform hardware reset if a reset pin is specified
    if (_rst != -1)
    {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, LOW);
        delay(10);
        digitalWrite(_rst, HIGH);
        delay(300);
    }

    // Disable auto-sleep
    i2c_write(0xFE, 0xFF); 
}

bool CST820::getTouch(uint16_t *x, uint16_t *y, uint8_t *gesture)
{
    uint8_t fingerDetected = i2c_read(0x02);

    if (fingerDetected == 0) {
        return false;
    }

    *gesture = i2c_read(0x01);
    
    uint8_t data[4];
    i2c_read_continuous(0x03, data, 4);
    *x = ((data[0] & 0x0F) << 8) | data[1];
    *y = ((data[2] & 0x0F) << 8) | data[3];

    return true;
}

uint8_t CST820::i2c_read(uint8_t addr)
{
    uint8_t rdData = 0;
    Wire.beginTransmission(I2C_ADDR_CST820);
    Wire.write(addr);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR_CST820, 1);
    while (Wire.available())
    {
        rdData = Wire.read();
    }
    return rdData;
}

uint8_t CST820::i2c_read_continuous(uint8_t addr, uint8_t *data, uint32_t length)
{
  Wire.beginTransmission(I2C_ADDR_CST820);
  Wire.write(addr);
  if (Wire.endTransmission(true)) return -1;
  Wire.requestFrom(I2C_ADDR_CST820, length);
  for (int i = 0; i < length; i++) {
    *data++ = Wire.read();
  }
  return 0;
}

void CST820::i2c_write(uint8_t addr, uint8_t data)
{
    Wire.beginTransmission(I2C_ADDR_CST820);
    Wire.write(addr);
    Wire.write(data);
    Wire.endTransmission();
}

uint8_t CST820::i2c_write_continuous(uint8_t addr, const uint8_t *data, uint32_t length)
{
  Wire.beginTransmission(I2C_ADDR_CST820);
  Wire.write(addr);
  for (int i = 0; i < length; i++) {
    Wire.write(*data++);
  }
  if (Wire.endTransmission(true)) return -1;
  return 0;
}