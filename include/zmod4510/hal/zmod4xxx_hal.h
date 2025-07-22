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
 * @file    zmod4xxx_hal.h
 * @brief   ZMOD4xxx specific hardware abstraction layer definitions
 *
 * This file serves as a wrapper header, enabling the use of the legacy 
 *  @ref zmod_api to be used with the new @ref hal_api, which is supporting
 *  different communication boards.
 *
 * @version 2.7.1
 * @author  Renesas Electronics Corporation
 *
 */

#ifndef _ZMOD4XXX_HAL_H_
#define _ZMOD4XXX_HAL_H_

#include "zmod4510/hal/hal.h"
#include "zmod4510/zmod4xxx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * Find the sensor and initialize hal specific data
 *
 * If example code is ported to the customer platform, this function must be
 *  re-implemented. The function must assign the zmod4xxx_dev_t#read, 
 *  zmod4xxx_dev_t#write and zmod4xxx_dev_t#delay_ms members of \a dev.
 *
 * \param    [in] dev   pointer to the sensor object
 * \param    [in] hal   pointer to the hal interface object
 * \return   error code
 * \retval   0 on success
 * \retval   !=0 hardware specific error code
 */
int  zmod4xxx_init ( zmod4xxx_dev_t*  dev, Interface_t*  hal );

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* _ZMOD4XXX_HAL_H_ */
