/*****************************************************************************
 * Copyright (c) 2024 Renesas Electronics Corporation
 * All Rights Reserved.
 * 
 * This code is proprietary to Renesas, and is license pursuant to the terms and
 * conditions that may be accessed at:
 * https://www.renesas.com/eu/en/document/msc/renesas-software-license-terms-gas-sensor-software
 *****************************************************************************/

/**
 * @file    zmod4510_config_no2_o3.h
 * @brief   This is the configuration for ZMOD4510 module - no2_o3 library
 * @version 1.0.1
 * @author  Renesas Electronics Corporation
 */

#ifndef _ZMOD4510_CONFIG_NO2_O3_H_
#define _ZMOD4510_CONFIG_NO2_O3_H_

#include <stdio.h>
#include "zmod4510/zmod4xxx_types.h"

#define INIT        0
#define MEASUREMENT 1

/**********************************/
/* < Define product ID > */
#define ZMOD4510_PID 0x6320

/**********************************/
/* < Define I2C slave address > */
#define ZMOD4510_I2C_ADDR 0x33

/**********************************/
/* < Define product data length > */
#define ZMOD4510_PROD_DATA_LEN 10

/*************************************/
/* < Define ADC result data length > */
#define ZMOD4510_ADC_DATA_LEN (32)

// time between samples
#define ZMOD4510_NO2_O3_SAMPLE_TIME (6000U)

#define ZMOD4XXX_H_ADDR 0x40
#define ZMOD4XXX_D_ADDR 0x50
#define ZMOD4XXX_M_ADDR 0x60
#define ZMOD4XXX_S_ADDR 0x68

extern uint8_t data_set_4510_init[];

extern uint8_t data_set_4510_no2_o3[];

extern zmod4xxx_conf zmod_no2_o3_sensor_cfg[];

#define RMOX3_OFFSET (15 * 2)

#endif //_ZMOD4510_CONFIG_NO2_O3_H_
