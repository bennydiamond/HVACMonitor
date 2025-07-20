/*****************************************************************************
 * Copyright (c) 2024 Renesas Electronics Corporation
 * All Rights Reserved.
 * 
 * This code is proprietary to Renesas, and is license pursuant to the terms and
 * conditions that may be accessed at:
 * https://www.renesas.com/eu/en/document/msc/renesas-software-license-terms-gas-sensor-software
 *****************************************************************************/

/**
 * @addtogroup hal_api
 * @{
 * @file    hal.c
 * @brief   Renesas Environmental Sensor HAL implementation
 * @version 2.7.1
 * @author  Renesas Electronics Corporation
 */

#include <stdio.h>
#include "zmod4510/hal/hal.h"

static struct {
  int                     error;
  int                     scope;
  ErrorStringGenerator_t  errStrFn;
} lastError;

int
HAL_SetError ( int  error, int  scope, ErrorStringGenerator_t  fn ) {
  lastError . error    = error;
  lastError . scope    = scope;
  lastError . errStrFn = fn;

  if ( scope == esSensor )
    return error;
  else
    return ecHALError;
}

char const*
HAL_GetErrorInfo ( int*  error, int*  scope, char*  str, int  bufLen ) {
  *error = lastError . error;
  *scope = lastError . scope;
  if ( str && bufLen ) {
    if ( lastError . errStrFn )
      lastError . errStrFn ( lastError . error, lastError . scope, str, bufLen );
    else
      snprintf ( str, bufLen, "No additional error information available" );
    return str;
  }
  else
    return NULL;
}


char const*
HAL_GetErrorString ( int  error, int scope, char*  str, int  bufLen ) {
  char buf [ 100 ];
  char const*  msg;
  switch ( error ) {
  case  heNoInterface:
    msg = "Interface not found";
    break;
  case  heNotImplemented:
    msg = "Function not implemented";
    break;
  case  heI2CReadMissing:
    msg = "I2CRead function pointer not set in interface object.";
    break;
  case  heI2CWriteMissing:
    msg = "I2CWrite function pointer not set in interface object.";
    break;
  case  heSleepMissing:
    msg = "msSleep function pointer not set in interface object.";
    break;
  case  heResetMissing:
    msg = "reset function pointer not set in interface object.";
    break;
  default:
    sprintf ( buf, "Unknown error %d", error );
    msg = buf;
  }
  snprintf ( str, bufLen, "HAL Error: %s", msg );
  return str;
}
