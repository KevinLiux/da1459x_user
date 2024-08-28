/**
 * \addtogroup PLA_BSP_SYSTEM
 * \{
 * \addtogroup PLA_MEMORY
 *
 * \{
 */

/**
 ****************************************************************************************
 *
 * @file psram_aps1604jsq.h
 *
 * @brief QSPI ram driver for aps1604jsq
 *
 *
 * Copyright (C) 2017-2021 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 *
 ****************************************************************************************
 */
#ifndef _PSRAM_APS1604JSQ_H_

#define _PSRAM_APS1604JSQ_H_

#define APS1604JSQ       0x5D

#ifndef APS1604_SIZE
#define APS1604_SIZE     (0x00 | APM_DENSITY_MASK)
#endif

#include "qspi_common.h"
#include "qspi_apmemory.h"

static void psram_aps1604jsq_sys_clock_cfg(HW_QSPIC_ID id, sys_clk_t sys_clk);

static const qspi_flash_config_t psram_aps1604jsq_config = {
        .manufacturer_id               = APMEMORY_ID,
        .device_type                   = APS1604JSQ,
        .device_density                = APS1604_SIZE,
        .initialize                    = psram_initialize,
        .sys_clk_cfg                   = psram_aps1604jsq_sys_clock_cfg,
        .get_dummy_bytes               = psram_get_dummy_bytes,
        .break_seq_size                = HW_QSPI_BREAK_SEQ_SIZE_1B,
        .address_size                  = HW_QSPI_ADDR_SIZE_24,
        .send_once                     = 0,
        .fast_read_opcode              = CMD_FAST_READ_QUAD,
        .enter_qpi_opcode              = APM_CMD_ENTER_QUAD,
        .extra_byte                    = 0x00,
        .qpi_mode                      = true,
        .is_ram                        = true,
        .burst_len                     = HW_QSPI2_MEMBLEN_64,
        .cs_active_time_max_us         = 8,
        .memory_size                   = MEMORY_SIZE_16Mb,
        .page_program_opcode           = CMD_WRITE_QUAD,
        .suspend_delay_us              = 0,     // Not applicable to PSRAM
        .resume_delay_us               = 0,     // Not applicable to PSRAM
        .reset_delay_us                = 50,
        .read_cs_idle_delay_ns         = 18,
        .erase_cs_idle_delay_ns        = 18,
};

__RETAINED_CODE static void psram_aps1604jsq_sys_clock_cfg(HW_QSPIC_ID id, sys_clk_t sys_clk)
{
#if dg_configUSE_HW_QSPI2
        psram_set_cs_active_max(id, sys_clk, psram_aps1604jsq_config.cs_active_time_max_us);
#endif
}
#endif /* _PSRAM_APS1604JSQ_H_ */
/**
 * \}
 * \}
 */
