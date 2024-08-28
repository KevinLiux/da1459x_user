/**
 ****************************************************************************************
 *
 * @file qspi_memory_config_table_internal.h
 *
 * @brief Header file which contains the QSPI memory configuration table
 *
 * When the memory autodetection functionality is enabled, the SDK reads the JEDEC ID of the
 * connected devices, and compares them with the JEDEC IDs of the QSPI flash/PSRAM drivers
 * contained in qspi_memory_config_table[]. If matched, the QSPIC/QSPIC2 are initialized with
 * the settings of the corresponding drivers(s).
 *
 * The SDK provides the option of implementing a custom qspi_memory_config_table[] in a new header
 * file. In this case, this file can be used as template. In order to build an application using the
 * custom qspi_memory_config_table[], the dg_configQSPI_MEMORY_CONFIG_TABLE_HEADER must be defined
 * with the name of aforementioned header file.
 *
 * Copyright (C) 2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef QSPI_FLASH_CONFIG_TABLE_INTERNAL_H_
#define QSPI_FLASH_CONFIG_TABLE_INTERNAL_H_

#if (dg_configFLASH_AUTODETECT == 1) || (dg_configQSPIC2_DEV_AUTODETECT == 1)

#include "qspi_gd25le32.h"
#include "qspi_mx25u3235.h"
#include "qspi_p25q32sl.h"
#include "qspi_w25q32fw.h"
#include "psram_aps1604jsq.h"
#include "psram_aps6404jsq.h"



const qspi_flash_config_t* qspi_memory_config_table[] = {
        &flash_gd25le32_config,
        &flash_mx25u3235_config,
        &flash_p25q32sl_config,
        &flash_w25q32fw_config,



        &psram_aps1604jsq_config,
        &psram_aps6404jsq_config,
};

#endif /* (dg_configFLASH_AUTODETECT == 1) || (dg_configQSPIC2_DEV_AUTODETECT == 1) */
#endif /* QSPI_FLASH_CONFIG_TABLE_INTERNAL_H_ */
