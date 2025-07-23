/**
  ******************************************************************************
  * @file    log_module.h
  * @author  MCD Application Team
  * @brief   Header file of the log module.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef LOG_MODULE_H
#define LOG_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <zephyr/logging/log.h>

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
/* Display 64 bits number for all compiler. */
#define LOG_DISPLAY64()             "0x%08X%08X"
#define LOG_NUMBER64( number )      (uint32_t)( number >> 32u ), (uint32_t)( number )

/* Module API - Log macros for each region */
/* LOG_REGION_BLE */
#define LOG_INFO_BLE(...)          LOG_INF(__VA_ARGS__)
#define LOG_ERROR_BLE(...)         LOG_ERR(__VA_ARGS__)
#define LOG_WARNING_BLE(...)       LOG_WRN(__VA_ARGS__)
#define LOG_DEBUG_BLE(...)         LOG_DBG(__VA_ARGS__)

/* LOG_REGION_MAC */
#define LOG_INFO_MAC(...)          LOG_INF(__VA_ARGS__)
#define LOG_ERROR_MAC(...)         LOG_ERR(__VA_ARGS__)
#define LOG_WARNING_MAC(...)       LOG_WRN(__VA_ARGS__)
#define LOG_DEBUG_MAC(...)         LOG_DBG(__VA_ARGS__)

/* LOG_REGION_SYSTEM */
#define LOG_INFO_SYSTEM(...)       LOG_INF(__VA_ARGS__)
#define LOG_ERROR_SYSTEM(...)      LOG_ERR(__VA_ARGS__)
#define LOG_WARNING_SYSTEM(...)    LOG_WRN(__VA_ARGS__)
#define LOG_DEBUG_SYSTEM(...)      LOG_DBG(__VA_ARGS__)

/* LOG_REGION_APP */
#define LOG_INFO_APP(...)          LOG_INF(__VA_ARGS__)
#define LOG_ERROR_APP(...)         LOG_ERR(__VA_ARGS__)
#define LOG_WARNING_APP(...)       LOG_WRN(__VA_ARGS__)
#define LOG_DEBUG_APP(...)         LOG_DBG(__VA_ARGS__)
#define LOG_ZIG_DBG(...)           LOG_DBG(__VA_ARGS__)

/* LOG_REGION_LINKLAYER */
#define LOG_INFO_LINKLAYER(...)    LOG_INF(__VA_ARGS__)
#define LOG_ERROR_LINKLAYER(...)   LOG_ERR(__VA_ARGS__)
#define LOG_WARNING_LINKLAYER(...) LOG_WRN(__VA_ARGS__)
#define LOG_DEBUG_LINKLAYER(...)   LOG_DBG(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOG_MODULE_H */
