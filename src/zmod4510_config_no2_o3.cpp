/*****************************************************************************
 * Copyright (c) 2024 Renesas Electronics Corporation
 * All Rights Reserved.
 * 
 * This code is proprietary to Renesas, and is license pursuant to the terms and
 * conditions that may be accessed at:
 * https://www.renesas.com/eu/en/document/msc/renesas-software-license-terms-gas-sensor-software
 *****************************************************************************/

/**
 * @file    zmod4510_config_no2_o3.cpp
 * @brief   This file contains the variable definitions for ZMOD4510 module - no2_o3 library
 * @version 1.0.1
 * @author  Renesas Electronics Corporation
 */

#include "zmod4510/algos/zmod4510_config_no2_o3.h"

// Define the variables that were previously defined in the header
uint8_t data_set_4510_init[] = {
                            0x00, 0x50,
                            0x00, 0x28, 0xC3, 0xE3,
                            0x00, 0x00, 0x80, 0x40};

uint8_t data_set_4510_no2_o3[] = {
                            0x00, 0x50, 0xFF, 0x06,
                            0xFE, 0xA2, 0xFE, 0x3E,
                            0x00, 0x10, 0x00, 0x52,
                            0x3F, 0x66, 0x00, 0x42,
                            0x23, 0x03,
                            0x00, 0x00, 0x02, 0x41,
                            0x00, 0x41, 0x00, 0x41,
                            0x00, 0x49, 0x00, 0x50,
                            0x02, 0x42, 0x00, 0x42,
                            0x00, 0x42, 0x00, 0x4A,
                            0x00, 0x50, 0x02, 0x43,
                            0x00, 0x43, 0x00, 0x43,
                            0x00, 0x43, 0x80, 0x5B,
                            };

zmod4xxx_conf zmod_no2_o3_sensor_cfg[] = {
    [INIT] = {
        .start = 0x80,
        .h = { .addr = ZMOD4XXX_H_ADDR, .len = 2, .data_buf = &data_set_4510_init[0]},
        .d = { .addr = ZMOD4XXX_D_ADDR, .len = 2, .data_buf = &data_set_4510_init[2]},
        .m = { .addr = ZMOD4XXX_M_ADDR, .len = 2, .data_buf = &data_set_4510_init[4]},
        .s = { .addr = ZMOD4XXX_S_ADDR, .len = 4, .data_buf = &data_set_4510_init[6]},
        .r = { .addr = 0x97, .len = 4},
    },

    [MEASUREMENT] = {
        .start = 0x80,
        .h = {.addr = ZMOD4XXX_H_ADDR, .len = 8, .data_buf = &data_set_4510_no2_o3[0]},
        .d = {.addr = ZMOD4XXX_D_ADDR, .len = 8, .data_buf = &data_set_4510_no2_o3[8]},
        .m = {.addr = ZMOD4XXX_M_ADDR, .len = 2, .data_buf = &data_set_4510_no2_o3[16]},
        .s = {.addr = ZMOD4XXX_S_ADDR, .len = 32, .data_buf = &data_set_4510_no2_o3[18]},
        .r = {.addr = 0x97, .len = 32},
        .prod_data_len = ZMOD4510_PROD_DATA_LEN,
    },
};