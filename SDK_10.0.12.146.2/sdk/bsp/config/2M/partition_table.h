/**
 ****************************************************************************************
 *
 * @file 2M/suota/partition_table.h
 *
 * Copyright (C) 2018-2019 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */


#define NVMS_PRODUCT_HEADER_PART_START  0x000000
#define NVMS_PRODUCT_HEADER_PART_SIZE   0x002000
#define NVMS_FW_EXEC_PART_START         0x002000        /* Alignment to 768KB is dictated by the default FLASH_REGION_SIZE. */
#define NVMS_FW_EXEC_PART_SIZE          0x0BE000

/* +----------------768KB---------------------+ */

#define NVMS_GENERIC_PART_START         0x0C0000
#define NVMS_GENERIC_PART_SIZE          0x020000
#define NVMS_PLATFORM_PARAMS_PART_START 0x0E0000
#define NVMS_PLATFORM_PARAMS_PART_SIZE  0x018000
#define NVMS_PARAM_PART_START           0x0F8000
#define NVMS_PARAM_PART_SIZE            0x001000        /* Recommended location, 4KB before the end of the 1st flash section. */

/* +------------------1MB---------------------+ */

#define NVMS_FW_UPDATE_PART_START       0x100000        /* Alignment to 768KB is dictated by the default FLASH_REGION_SIZE. */
#define NVMS_FW_UPDATE_PART_SIZE        0x0C0000        /* This size is dictated by the default (768KB) FLASH_REGION_SIZE. */
#define NVMS_LOG_PART_START             0x1C0000
#define NVMS_LOG_PART_SIZE              0x028000
#define NVMS_PARTITION_TABLE_START      0x1F8000
#define NVMS_PARTITION_TABLE_SIZE       0x001000        /* Recommended location, 4KB before the end of the flash. */

PARTITION2( NVMS_PRODUCT_HEADER_PART  , 0 )
PARTITION2( NVMS_FW_EXEC_PART         , 0 )
PARTITION2( NVMS_GENERIC_PART         , PARTITION_FLAG_VES )
PARTITION2( NVMS_PLATFORM_PARAMS_PART , PARTITION_FLAG_READ_ONLY )
PARTITION2( NVMS_PARAM_PART           , 0 )
PARTITION2( NVMS_FW_UPDATE_PART       , 0 )
PARTITION2( NVMS_LOG_PART             , 0 )
PARTITION2( NVMS_PARTITION_TABLE      , PARTITION_FLAG_READ_ONLY )


