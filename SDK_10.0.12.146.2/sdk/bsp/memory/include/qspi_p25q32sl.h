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
 * @file qspi_p25q32sl.h
 *
 * @brief Driver for flash PUYA P25Q32SL
 *
 * Copyright (C) 2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef _QSPI_PUYA_P25Q32SL_H_
#define _QSPI_PUYA_P25Q32SL_H_

#include "qspi_common.h"
#include "sdk_defs.h"

/************************************************* JEDEC ID INFO *************************************************/

/**
 * \brief The Flash type JEDEC ID
 *
 * This is the first byte returned by the 0x9F command
 */
#define PUYA_ID                                                 0x85

/**
 * \brief The Flash type JEDEC ID
 *
 * This is the second byte returned by the 0x9F command
 */
#define P25Q32SL_TYPE                                           0x60

/**
 * \brief The Flash density JEDEC ID
 *
 * This is the third byte returned by the 0x9F command
 */
#define P25Q32SL_SIZE                                           0x16

/**************************************************** TIMINGS ****************************************************/

/* Flash power up/down timings in usec - cannot have less than 3us delay */
#define P25Q32SL_POWER_DOWN_DELAY_US                            3
#define P25Q32SL_RELEASE_POWER_DOWN_DELAY_US                    8
#define P25Q32SL_POWER_UP_DELAY_US                              12000
#define P25Q32SL_SUSPEND_DELAY_US                               30
#define P25Q32SL_RESUME_DELAY_US                                30
#define P25Q32SL_RESET_DELAY_US                                 12000
#define P25Q32SL_READ_CS_IDLE_NS                                20
#define P25Q32SL_ERASE_CS_IDLE_NS                               30

/************************************************ Opcodes SECTION ************************************************/

/* Status Register Opcodes */
#define P25Q32SL_READ_SR2_REGISTER_OPCODE                       0x35
#define P25Q32SL_WRITE_SR2_REGISTER_OPCODE                      0x31
#define P25Q32SL_READ_CR_REGISTER_OPCODE                        0x15
#define P25Q32SL_WRITE_CR_REGISTER_OPCODE                       0x11

/* Suspend/Resume Opcodes */
#define P25Q32SL_ERASE_PROGRAM_SUSPEND_OPCODE                   0x75
#define P25Q32SL_ERASE_PROGRAM_RESUME_OPCODE                    0x7A

/********************************************** DRIVER GENERIC INFO **********************************************/

/* Quad Enable Bit Position - Status Register 2 */
#define P25Q32SL_SR2_QE_BIT_POS                                 (1)
#define P25Q32SL_SR2_QE_MASK                                    (1 << P25Q32SL_SR2_QE_BIT_POS)

/* Erase/Program Suspend Bit Positions - Status Register 2 */
#define P25Q32SL_SR2_EPSUS_BIT_POS                              (7)
#define P25Q32SL_SR2_EPSUS_MASK                                 (1 << P25Q32SL_SR2_EPSUS_BIT_POS)

/* Dummy Cycles Field Position - Configure Register                             *
 * Bit State    Instruction     Dummy Bytes(Extra Byte Incl)    Max Freq(MHz)   *
 * 0            EB              6                               70              *
 * 1            EB              10                              85+             */
#define P25Q32SL_CR_DC_BIT_POS                                  (1)
#define P25Q32SL_CR_DC_MASK                                     (1 << P25Q32SL_CR_DC_BIT_POS)

/* Extra/Mode Byte used for entering continuous read mode */
#define P25Q32SL_EXTRA_BYTE_M7_M0                               0xA5

/* Enumerator for the Dummy Cycle (DC) bit of the Configuration Register, which is used to      *
 * configure the number of the dummy bytes for SPI 4X I/O Read (EBH) command.                   */
typedef enum {
        P25Q32SL_EB_CONFIG_REG_DC_0 = 0x00,
        P25Q32SL_EB_CONFIG_REG_DC_1 = 0x01,
} P25Q32SL_EB_CONFIG_REG_DC;


/********************************************** FUNCTION PROTOTYPES **********************************************/
/**
 * \brief Set the Quad Enable Bit of the Status Register if it is not already set.
 *
 * \param[in] id QSPI controller id
 */
__RETAINED_CODE static void flash_p25q32sl_enable_quad_mode(HW_QSPIC_ID id);

/**
 * \brief This function returns the dummy bytes required when utilizing Fast Read Quad command.
 *
 * \param[in] id QSPI controller id
 * \param[in] sys_clk system clock
 *
 * \return Dummy bytes
 */
__RETAINED_CODE static uint8_t flash_p25q32sl_get_dummy_bytes(HW_QSPIC_ID id, sys_clk_t sys_clk);

/**
 * \brief Initialize P25Q32SL QSPI Flash
 *
 * \param[in] id QSPI controller id
 *
 */
__RETAINED_CODE static void flash_p25q32sl_initialize(HW_QSPIC_ID id);

/**
 * \brief check if an erase or program operation is suspended.
 *
 * \param[in] id QSPI controller id
 *
 * \return True, if the flash is suspended, otherwise false.
 */
__RETAINED_CODE static bool flash_p25q32sl_is_suspended(HW_QSPIC_ID id);

/**
 * \brief Read the Status Register 2.
 *
 * \param[in] id QSPI controller id
 *
 * \return The value of the Status Register 2.
 */
__RETAINED_CODE static uint8_t flash_p25q32sl_read_status_register_2(HW_QSPIC_ID id);

/**
 * \brief Read the Configure Register.
 *
 * \param[in] id QSPI controller id
 *
 * \return The value of the Configure Register.
 */
__RETAINED_CODE static uint8_t flash_p25q32sl_read_configure_register(HW_QSPIC_ID id);

/**
 * \brief Sets up the Read Operation expected Dummy Bytes.
 *
 * \param[in] id QSPI controller id
 * \param[in] sys_clk System clock
 *
 * \note This function depending on bit_state will either set or clear the configure register DC bit
 *       of the Flash device so the device expects 8 dummy cycles for BBh command and 10 dummy cycles
 *       for EBh command, extra byte included. This effectively allows the Flash to operate on higher
 *       than 70 MHz frequencies without compromising the integrity of the data received.
 */
__RETAINED_CODE static void flash_p25q32sl_set_dummy_bytes(HW_QSPIC_ID id, sys_clk_t sys_clk);

/**
 * \brief Hook function called before changing the system clock.
 *
 * \param[in] id QSPI controller id
 * \param[in] sys_clk system clock
 *
 */
__RETAINED_CODE static void flash_p25q32sl_sys_clock_cfg(HW_QSPIC_ID id, sys_clk_t sys_clk);

/**
 * \brief Write the Status Register 2.
 *
 * \param[in] id QSPI controller id
 * \param[in] value The value to be written.
 *
 * \note This function blocks until the Flash has processed the command. No verification that the
 *       value has been actually written is done though. It is up to the caller to decide whether
 *       such verification is needed or not and execute it on its own.
 */
__RETAINED_CODE static void flash_p25q32sl_write_status_register_2(HW_QSPIC_ID id, uint8_t value);

/**
 * \brief Write the Configure Register of the Flash
 *
 * \param[in] id QSPI controller id
 * \param[in] value The value to be written.
 *
 * \note This function blocks until the Flash has processed the command. No verification that the
 *       value has been actually written is done though. It is up to the caller to decide whether
 *       such verification is needed or not and execute it on its own.
 */
__RETAINED_CODE static void flash_p25q32sl_write_configure_register(HW_QSPIC_ID id, uint8_t value);

/************************************************* CONFIG OBJECT *************************************************/

/**
 * \brief This struct configures the system for the specific flash
 *
 * \note This struct MUST be const for this to work. Therefore, assignments must
 *       not change (must be read-only)
 */
static const qspi_flash_config_t flash_p25q32sl_config = {
        /* JEDEC Bytes 9Fh */
        .manufacturer_id                                = PUYA_ID,
        .device_type                                    = P25Q32SL_TYPE,
        .device_density                                 = P25Q32SL_SIZE,

        /* Flash Info */
        .memory_size                                    = (32 * MEMORY_SIZE_1Mb),
        .address_size                                   = HW_QSPI_ADDR_SIZE_24,
        .is_ram                                         = false,
        .qpi_mode                                       = false,

        /* Callbacks */
        .is_suspended                                   = flash_p25q32sl_is_suspended,
        .initialize                                     = flash_p25q32sl_initialize,
        .sys_clk_cfg                                    = flash_p25q32sl_sys_clock_cfg,
        .get_dummy_bytes                                = flash_p25q32sl_get_dummy_bytes,

        /* Read */
        .fast_read_opcode                               = CMD_FAST_READ_QUAD,
        .send_once                                      = 1,
        .extra_byte                                     = P25Q32SL_EXTRA_BYTE_M7_M0,
        .break_seq_size                                 = HW_QSPI_BREAK_SEQ_SIZE_1B,

        /* Page Program */
        .page_program_opcode                            = CMD_QUAD_PAGE_PROGRAM,
        .quad_page_program_address                      = false,

        /* Sector Erase */
        .erase_opcode                                   = CMD_SECTOR_ERASE,
        .read_erase_progress_opcode                     = CMD_READ_STATUS_REGISTER,
        .erase_in_progress_bit                          = FLASH_STATUS_BUSY_BIT,
        .erase_in_progress_bit_high_level               = true,

        /* Program/Erase Suspend/Resume */
        .erase_suspend_opcode                           = P25Q32SL_ERASE_PROGRAM_SUSPEND_OPCODE,
        .erase_resume_opcode                            = P25Q32SL_ERASE_PROGRAM_RESUME_OPCODE,

        /* Timings */
        .power_down_delay                               = P25Q32SL_POWER_DOWN_DELAY_US,
        .release_power_down_delay                       = P25Q32SL_RELEASE_POWER_DOWN_DELAY_US,
        .power_up_delay                                 = P25Q32SL_POWER_UP_DELAY_US,
        .suspend_delay_us                               = P25Q32SL_SUSPEND_DELAY_US,
        .resume_delay_us                                = P25Q32SL_RESUME_DELAY_US,
        .reset_delay_us                                 = P25Q32SL_RESET_DELAY_US,
        .read_cs_idle_delay_ns                          = P25Q32SL_READ_CS_IDLE_NS,
        .erase_cs_idle_delay_ns                         = P25Q32SL_ERASE_CS_IDLE_NS,
};

/********************************************* FUNCTION  DEFINITIONS *********************************************/

__RETAINED_CODE static void flash_p25q32sl_enable_quad_mode(HW_QSPIC_ID id)
{
        uint8_t status;

        status = flash_p25q32sl_read_status_register_2(id);
        if (!(status & P25Q32SL_SR2_QE_MASK))
        {
                flash_write_enable(id);
                flash_p25q32sl_write_status_register_2(id, status | P25Q32SL_SR2_QE_MASK);
        }
}

// The
__RETAINED_CODE static uint8_t flash_p25q32sl_get_dummy_bytes(__UNUSED HW_QSPIC_ID id, sys_clk_t sys_clk)
{
        switch (sys_clk) {
        case sysclk_RC32:
        case sysclk_XTAL32M:
                return 2;
        case sysclk_PLL96:
                return 4;
        default:
                ASSERT_ERROR(0);
                return 0;
        }
}

__RETAINED_CODE static void flash_p25q32sl_initialize(HW_QSPIC_ID id)
{
        /* Set QE Bit if it is not set */
        flash_p25q32sl_enable_quad_mode(id);
        flash_p25q32sl_set_dummy_bytes(id, sysclk_RC32);
}

__RETAINED_CODE static bool flash_p25q32sl_is_suspended(HW_QSPIC_ID id)
{
        __DBG_QSPI_VOLATILE__ uint8_t status;

        status = flash_p25q32sl_read_status_register_2(id);
        return ((status & P25Q32SL_SR2_EPSUS_MASK) != 0);
}

__RETAINED_CODE static uint8_t flash_p25q32sl_read_status_register_2(HW_QSPIC_ID id)
{
        __DBG_QSPI_VOLATILE__ uint8_t status;
        uint8_t cmd[] = { P25Q32SL_READ_SR2_REGISTER_OPCODE };

        flash_transact(id, cmd, 1, &status, 1);

        return status;
}

__RETAINED_CODE static uint8_t flash_p25q32sl_read_configure_register(HW_QSPIC_ID id)
{
        __DBG_QSPI_VOLATILE__ uint8_t status;
        uint8_t cmd[] = { P25Q32SL_READ_CR_REGISTER_OPCODE };

        flash_transact(id, cmd, 1, &status, 1);

        return status;
}

__RETAINED_CODE static void flash_p25q32sl_set_dummy_bytes(HW_QSPIC_ID id, uint8_t sys_clk)
{
        uint8_t dummy_bytes;
        uint8_t config_reg;
        uint8_t config_reg_dc;

        dummy_bytes = flash_p25q32sl_get_dummy_bytes(id, sys_clk);

        switch (dummy_bytes) {
        case 2:
                config_reg_dc = (uint8_t) P25Q32SL_EB_CONFIG_REG_DC_0;
                break;
        case 4:
                config_reg_dc = (uint8_t) P25Q32SL_EB_CONFIG_REG_DC_1;
                break;
        default:
                ASSERT_ERROR(0);
                return;
        }

        config_reg_dc = config_reg_dc << P25Q32SL_CR_DC_BIT_POS;
        config_reg = flash_p25q32sl_read_configure_register(id);

        if ((config_reg & P25Q32SL_CR_DC_MASK) != config_reg_dc) {
                config_reg_dc = ((config_reg & ~P25Q32SL_CR_DC_MASK) | config_reg_dc);

                flash_write_enable(id);
                flash_p25q32sl_write_configure_register(id, config_reg_dc);
        }

        hw_qspi_set_dummy_bytes_count(id, dummy_bytes);
}

__RETAINED_CODE static void flash_p25q32sl_sys_clock_cfg(HW_QSPIC_ID id, sys_clk_t sys_clk)
{
        qspi_int_activate_command_entry_mode(id);
        flash_p25q32sl_set_dummy_bytes(id, sys_clk);
        qspi_int_deactivate_command_entry_mode(id);
}

__RETAINED_CODE static void flash_p25q32sl_write_status_register_2(HW_QSPIC_ID id, uint8_t value)
{
        uint8_t cmd[2] = { P25Q32SL_WRITE_SR2_REGISTER_OPCODE, value };

        flash_write(id, cmd, 2);

        /* Wait for the Flash to process the command */
        while (flash_is_busy(id));
}

__RETAINED_CODE static void flash_p25q32sl_write_configure_register(HW_QSPIC_ID id, uint8_t value)
{
        uint8_t cmd[2] = { P25Q32SL_WRITE_CR_REGISTER_OPCODE, value };

        flash_write(id, cmd, 2);

        /* Wait for the Flash to process the command */
        while (flash_is_busy(id));
}

#if (dg_configUSE_SEGGER_FLASH_LOADER == 1) && (dg_configFLASH_AUTODETECT == 0)
__attribute__((used, __section__("__product_header_primary__")))
static const PRODUCT_HEADER_STRUCT(3) ph_primary = {
        .burstcmdA = 0xA8A500EB,
        .burstcmdB = 0x00002076,
        .flash_config_section = 0x11AA,
        .flash_config_length = 0x3,
        .config_seq = {0x11, 0x02, 0x07},
        .crc = 0x0A35
};

__attribute__((used, __section__("__product_header_backup__")))
static const PRODUCT_HEADER_STRUCT(3) ph_backup = {
        .burstcmdA = 0xA8A500EB,
        .burstcmdB = 0x00002076,
        .flash_config_section = 0x11AA,
        .flash_config_length = 0x3,
        .config_seq = {0x11, 0x02, 0x07},
        .crc = 0x0A35
};
#endif /* (dg_configUSE_SEGGER_FLASH_LOADER == 1) &&  (dg_configFLASH_AUTODETECT == 0) */

#endif /* _QSPI_PUYA_P25Q32SL_H_ */
/**
 * \}
 * \}
 */
