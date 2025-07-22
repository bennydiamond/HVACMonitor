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
 * @file    hal.h
 * @brief   Renesas Environmental Sensor HAL definitions
 * @version 2.7.1
 * @author  Renesas Electronics Corporation
 * 
 * This header defines the HAL interface. Functions declared here have to be
 *  re-implemented when code is ported to a new platform.
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>

/**
 * @brief Error code definitions
 */
typedef enum {
  ecSuccess  = 0,           /**< Operation completed successfully */
  ecHALError = 0x100        /**< Returned by sensor API if a HAL function failed. 
                             * Specific information about the error can be 
                             * obtained using the function HAL_GetErrorInfo(). */
} GenericError_t;

/**
 * @brief Success status code and error scopes
 * 
 */
typedef enum {
  esSensor    = 0x0000,   /**< Sensor scope */
  esAlgorithm = 0x1000,   /**< Algorithm scope */
  esInterface = 0x2000,   /**< Interface scope */
  esHAL       = 0x3000,   /**< HAL scope */
} ErrorScope_t;

/**
 * @brief HAL scope error definitions
 * 
 * When sensors are initialized (e.g. init_hardware()), the hal objects is
 *  checked whether all HAL functions required by the sensor are provided.
 *  If a function is missing one of the errors from this enumeration is
 *  returned.
 */
typedef enum {
  heNoInterface = 1,     /**< There was no interface found */
  heNotImplemented,      /**< The requested function is not implemented */
  heI2CReadMissing,      /**< Interface_t::i2cRead not provided */
  heI2CWriteMissing,     /**< Interface_t::i2cWrite not provided */
  heSleepMissing,        /**< Interface_t::msSleep not provided */
  heResetMissing         /**< Interface_t::reset not provided */
} HALError_t;


/**
 * @brief Function pointer type defining signature of I2C functions.
 * 
 * This function pointer typedef is used in Interface_t objects to hold
 *  pointers to I2C implementations of read and write. 
 */
typedef int ( *I2CImpl_t ) ( void*, uint8_t, uint8_t*, int, uint8_t*, int );


/**
 * @brief A structure of pointers to hardware specific functions
 */
typedef struct {
  /** handle to physical interface */
  void*  handle;
  
  /** Pointer to I2C read implementation
   *
   * The read operation may be preceded by a write to the same slave
   *  address. The function accepts a pointer to the interface handle, the
   *  slave address of the device to communicate with and two pairs of 
   *  buffer pointer and buffer length.
   * The first pair of buffer pointer / buffer length defines the data to
   *  be written, the second pair provides the pointer to the buffer where
   *  received data is stored and how many bytes shall be read.
   * If the length field of the first buffer is not zero, the implementation
   *  must send an I2C write condition on the bus, transfer the data and send
   *  a repeated start condition followed by the corresponding read 
   *  condition on the bus.
   * If the length parameter of the first buffer is zero, no write is
   *  generated and the read is started immediately.
   * The transmission must be terminated with a stop condition on the bus.
   */
  I2CImpl_t   i2cRead;
  
  /** Pointer to I2C write implementation 
   * 
   * For convenience, the write operation accepts two pairs of buffer pointer
   * and buffer length. This allows to transfer addresses or commands prior 
   * to data without the need to manually concatenate transmit data in a
   * buffer.
   * The implementation of this function must generate a start condition
   *  followed by the I2C slave address of the target device, followed by all
   *  data from both buffers.
   * At the end of the transmission a stop bit must be generated on the bus.
   * 
   */
  I2CImpl_t   i2cWrite;
  
  /** Pointer to delay function
   * 
   * An implementation must delay execution by the specified number of `ms`
   */
  void ( *msSleep ) ( uint32_t  ms );

  /** Pointer to reset function
   * 
   * Implementation must pulse the reset pin
   */
  int  ( *reset ) ( void*  handle );
} Interface_t;


/**
 * @brief Initialize hardware and populate ::Interface_t object
 * 
 * Any implementation must initialize those members of the ::Interface_t object that
 *  are required by the sensor being operated with pointers to functions that
 *  implement the behavior as specified in the ::Interface_t member documentation.
 * 
 * @param hal   pointer to ::Interface_t object to be initialized
 * @return      error code
 * @retval  0   on success
 * @retval !=0  in case of error
 */
int  HAL_Init ( Interface_t*  hal );

/**
 * @brief Cleanup before program exit
 * 
 * This function shall free up resources that have been allocated through
 * HAL_Init().
 * 
 * @param hal   pointer to ::Interface_t object to be deinitialized
 * @return      error code
 * @retval  0   on success
 * @retval !=0  in case of error
 */
int  HAL_Deinit ( Interface_t*  hal );

/**
 * @brief Error handling function
 * 
 * The implementation of this function defines the behavior of the
 * application code when an error occurs during execution.
 * 
 * @param errorCode code of the error to be handled
 * @param context   additional context information
 */
void HAL_HandleError ( int  errorCode, void const*  context );

/**
 * @brief Function type used for generation of error strings
 * 
 * Functions of this type may be passed to HAL_SetError() to generate
 *  meaningful descriptions of error conditions.
 * 
 */
typedef char const*  ( *ErrorStringGenerator_t ) ( int, int, char*, int );

/**
 * @brief Function storing error information
 * 
 * The sensor interface has different components which may generate error
 *  conditions. To keep the sensor interface as simple as possible, this
 *  function is called in case of an error. The return value of this
 *  function will be returned as error code of the function in which an
 *  error has occurred.
 * 
 * Internally, this function stores the error code and the scope of the
 *  error (that is which module was generating the error) in a data
 *  structure. For all errors which do not have the scope esSensor, this
 *  function will return the generic error code ecHALError. Error codes
 *  generated by the sensor are returned directly.
 * 
 * It is possible to pass an error string generator function along with
 *  the error information. If this function is provided, the error handler
 *  can query an error string, providing more meaningful error information.
 * 
 * @param error   An error code
 * @param scope   The scope of the error (integer identifying a module)
 * @param errStrFn  Optional function pointer that can decode generate a 
 *                  meaningful message for the error code. 
 *                  Pass NULL if not used.
 * 
 */
int HAL_SetError ( int  error, int  scope, 
                   ErrorStringGenerator_t  errStrFn );

/**
 * @brief Get detailed information for last error
 * 
 * Use this function in the error handler to obtain information about the 
 *  last error code and which component generated it. In addition if 
 *  an error string generator function was provided during error generation,
 *  this function may return a text string, describing the error in more
 *  detail.
 * 
 *  @param error    Pointer to integer, where the error code is written
 *  @param scope    Pointer to integer, where the error scope (module that was
 *                  generating the error is written)
 *  @param str      Pointer to string buffer, where error message is written.
 *                  If no string information is required, pass NULL pointer.
 *  @param bufSize  Size of the string buffer, pass 0 if not used
 *  @return         Value passed in str
 */
char const*  HAL_GetErrorInfo ( int*  error, int*  scope, char*  str, int  bufSize );

/**
 * @brief Error string generator for HAL-scoped errors
 * 
 * This function generates error information for HAL scoped errors. Usually
 *  user code does not need to use this function directly.
 * 
 *  @param error    Error code for which error information is to be returned
 *  @param scope    Error scope for which error information is to be returned
 *  @param str      Pointer to string buffer, where error message is written
 *  @param bufSize  Size of the string buffer
 *  @return         Value passed in str
 */
char const*  HAL_GetErrorString ( int  error, int  scope, char*  str, int  bufSize );

#endif /* HAL_H */
/** @} */
