/*****************************************************************************
 * Copyright (c) 2024 Renesas Electronics Corporation
 * All Rights Reserved.
 * 
 * This code is proprietary to Renesas, and is license pursuant to the terms and
 * conditions that may be accessed at:
 * https://www.renesas.com/eu/en/document/msc/renesas-software-license-terms-gas-sensor-software
 *****************************************************************************/

/**
 * @file    araduino.cpp
 * @brief   I2C wrapper functions for Arduino
 * @version 2.7.1
 * @author  Renesas Electronics Corporation
 */

#include "zmod4510/hal/hal.h"
#include <Arduino.h>
#include <Wire.h>
#include <stdio.h>
#include "zmod4510/hal/arduino/arduino_hal.h"
#include "I2CBridge.h"
#include "Logger.h"
#include "SerialMutex.h"

static char const*
_GetErrorString ( int  error, int  scope, char*  str, int  strLen ) {
  char  buf [ 30 ];
  char const*  err;
  switch ( error ) {
  case 1:
    err = "Data too long to fit in transmit buffer";
    break;
  case 2:
    err = "Received NACK on transmit of address";
    break;
  case 3:
    err = "Received NACK on transmit of data";
    break;
  case 4:
    err = "Other error";
    break;
  case 5:
    err = "Timeout";
    break;
  default:
    sprintf ( buf, "Unkown error %d", error );
    err = buf;
  }
  snprintf ( str, strLen, "Arduino Wire Error: %s", err );
  return str;
}

static void
_Delay ( uint32_t ms ) {
  delay ( ms );
}

static int
_I2CRead ( void*  ifce, uint8_t  slAddr, uint8_t*  wrData, int  wrSize, uint8_t*  rdData, int  rdSize ) {
  ( void) ifce;
  
  
  if (wrSize > 0 && wrData != nullptr) {
    String hexData;
    for (int i = 0; i < wrSize; i++) {
      if (i > 0) hexData += " ";
      char hex[4];
      sprintf(hex, "%02X", wrData[i]);
      hexData += hex;
    }
  }
  
  I2CBridge& i2c = I2CBridge::getInstance();
  I2CBridge::Result result;
  
  if (wrSize > 0) {
    // Use write-then-read operation
    result = i2c.writeReadBytes(slAddr, wrData, wrSize, rdSize);
  } else {
    // Simple read operation
    result = i2c.readBytes(slAddr, rdSize);
  }
  
  if (!result.success) {
    logger.warningf("ZMOD4510: I2CRead failed with error code: 0x%02X", result.error_code);
    return HAL_SetError(result.error_code, aesArduino, _GetErrorString);
  }
  
  // Copy the data to the output buffer
  memcpy(rdData, result.data, result.data_len);
  
  // Log the read data
  String hexData;
  for (int i = 0; i < result.data_len; i++) {
    if (i > 0) hexData += " ";
    char hex[4];
    sprintf(hex, "%02X", result.data[i]);
    hexData += hex;
  }
  
  return ecSuccess;
}


static int
_I2CWrite( void*  ifce, uint8_t  slAddr, uint8_t*  wrData1, int  wrSize1, uint8_t*  wrData2, int  wrSize2 ) {
  ( void) ifce;  
  // Log the write data buffers
  if (wrSize1 > 0 && wrData1 != nullptr) {
    String hexData;
    for (int i = 0; i < wrSize1; i++) {
      if (i > 0) hexData += " ";
      char hex[4];
      sprintf(hex, "%02X", wrData1[i]);
      hexData += hex;
    }
  }
  
  if (wrSize2 > 0 && wrData2 != nullptr) {
    String hexData;
    for (int i = 0; i < wrSize2; i++) {
      if (i > 0) hexData += " ";
      char hex[4];
      sprintf(hex, "%02X", wrData2[i]);
      hexData += hex;
    }
  }
  
  I2CBridge& i2c = I2CBridge::getInstance();
  I2CBridge::Result result;
  
  // Combine the two data buffers if needed
  if (wrSize1 > 0 && wrSize2 > 0) {
    uint8_t combinedData[64]; // Assuming max 64 bytes total
    if (wrSize1 + wrSize2 > 64) {
      logger.error("ZMOD4510: I2CWrite - Combined buffer too large");
      return HAL_SetError(5, aesArduino, _GetErrorString); // Buffer too small
    }
    
    // Copy both buffers into the combined buffer
    memcpy(combinedData, wrData1, wrSize1);
    memcpy(combinedData + wrSize1, wrData2, wrSize2);
    
    // Write the combined data
    result = i2c.writeBytes(slAddr, combinedData, wrSize1 + wrSize2);
  } else if (wrSize1 > 0) {
    // Just write the first buffer
    result = i2c.writeBytes(slAddr, wrData1, wrSize1);
  } else if (wrSize2 > 0) {
    // Just write the second buffer
    result = i2c.writeBytes(slAddr, wrData2, wrSize2);
  } else {
    // No data to write, just send address
    uint8_t dummy = 0;
    result = i2c.writeBytes(slAddr, &dummy, 0);
  }
  
  if (!result.success) {
    logger.warningf("ZMOD4510: I2CWrite failed with error code: 0x%02X", result.error_code);
    return HAL_SetError(result.error_code, aesArduino, _GetErrorString);
  }
  
  return ecSuccess;
}



int
HAL_Init ( Interface_t*  hal ) {
  logger.info("ZMOD4510: Initializing HAL interface");
  Wire . begin ( );
  hal -> i2cRead     = _I2CRead;
  hal -> i2cWrite    = _I2CWrite;
  hal -> msSleep     = _Delay;
  hal -> reset       = NULL;

  logger.info("ZMOD4510: HAL interface initialized successfully");
  return ecSuccess;
}

int
HAL_Deinit      ( Interface_t*  hal ) {
  return ecSuccess;
}

void
HAL_HandleError ( int  errorCode, void const*  contextV ) {
  char const* context = ( char const* ) contextV;
  int  error, scope;
  char  msg [ 200 ];
  if ( errorCode ) {
    HAL_GetErrorInfo(&error, &scope, msg, 200);
    logger.errorf("Error %d received during %s: %s", errorCode, context, msg);
  }
  while ( 1 );
}

