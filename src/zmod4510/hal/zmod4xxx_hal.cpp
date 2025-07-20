/*****************************************************************************
 * Copyright (c) 2024 Renesas Electronics Corporation
 * All Rights Reserved.
 * 
 * This code is proprietary to Renesas, and is license pursuant to the terms and
 * conditions that may be accessed at:
 * https://www.renesas.com/eu/en/document/msc/renesas-software-license-terms-gas-sensor-software
 *****************************************************************************/

/**
 * @file    zmod4xxx_hal.c
 * @brief   zmod4xxx hardware abstraction layer function definitions
 * @version 2.7.1
 * @author  Renesas Electronics Corporation
 */

#include <string.h>   /* requried for memcpy declaration */

#include "zmod4510/hal/hal.h"
#include "zmod4510/hal/zmod4xxx_hal.h"
#include "zmod4510/sensors/zmod4xxx_types.h"

static Interface_t* _hal;

/* wrapper function, mapping register read api to generic I2C API */
static int8_t
_i2c_read_reg ( uint8_t  slaveAddr, uint8_t  addr, uint8_t*  data, uint8_t  len ) {
  return _hal -> i2cRead ( _hal -> handle, slaveAddr, &addr, 1, data, len );
}


/* wrapper function, mapping register write api to generic I2C API */
static int8_t
_i2c_write_reg ( uint8_t  slaveAddr, uint8_t  addr, uint8_t*  data, uint8_t  len ) {
  return _hal -> i2cWrite ( _hal -> handle, slaveAddr, &addr, 1, data, len );
}


int
zmod4xxx_init ( zmod4xxx_dev_t*  dev, Interface_t*  hal ) {
  uint8_t  dummy [ 1 ];

  /* verify we have all functions required for the ZMOD4xxx API */
  if ( !hal -> i2cRead ) {
    HAL_SetError ( heI2CReadMissing, esHAL, HAL_GetErrorString );
    return ERROR_NULL_PTR;
  }
  
  if ( !hal -> i2cWrite ) {
    HAL_SetError ( heI2CWriteMissing, esHAL, HAL_GetErrorString );
    return ERROR_NULL_PTR;
  }
  
  if ( !hal -> msSleep ) {
    HAL_SetError ( heSleepMissing, esHAL, HAL_GetErrorString );
    return ERROR_NULL_PTR;
  }

  
  /* populate function pointers in legacy ZMOD4xxx API */
  dev -> write    = _i2c_write_reg;
  dev -> read     = _i2c_read_reg;
  dev -> delay_ms = hal -> msSleep;
  
  dev -> delay_ms ( 200 );
  
  _hal = hal;

  /* verify there is a sensor connected */
  if ( hal -> i2cWrite ( hal -> handle, dev ->i2c_addr, dummy, 0, NULL, 0 ) ) {
    return ERROR_I2C;
  }

  return ZMOD4XXX_OK;
}
