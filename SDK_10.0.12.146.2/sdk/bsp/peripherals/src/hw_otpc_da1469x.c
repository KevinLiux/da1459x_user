/**
 * \addtogroup BSP
 * \{
 * \addtogroup DEVICES
 * \{
 * \addtogroup OTPC
 * \{
 */

/**
 ****************************************************************************************
 *
 * @file hw_otpc_da1469x.c
 *
 * @brief Implementation of the OTP Controller Low Level Driver
 *
 * Copyright (C) 2017-2021 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#if dg_configUSE_HW_OTPC

#include "hw_otpc.h"
#include "hw_clk.h"

/*
 * Local variables
 */
/* Add specific TIM1 settings
 *  TIM1_CC_T_1US value =  (1000ns * N Mhz / 1000) - 1
 *  TIM1_CC_T_10NS value =  (20ns *  N Mhz / 1000) - 1
 *  TIM1_CC_T_RD value =  (120ns *  N Mhz / 1000) - 1
 *
 */
static const uint32_t tim1[] = {
        /* 2MHz */
        ( 0x01 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_1US_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_20NS_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_RD_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_PL_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CS_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CSP_Pos ),
        /* 4MHz */
        ( 0x03 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_1US_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_20NS_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_RD_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_PL_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CS_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CSP_Pos ),
        /* 8MHz */
        ( 0x07 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_1US_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_20NS_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_RD_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_PL_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CS_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CSP_Pos ),
        /*  16MHz*/
        ( 0x0F << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_1US_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_20NS_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_RD_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_PL_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CS_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CSP_Pos ),

        /* default 32MHz*/
        ( 0x1F << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_1US_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_20NS_Pos ) |
        ( 0x01 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_RD_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_PL_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CS_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CSP_Pos ),
        /*  48MHz is not supported for DA1469X device family as PLL is only allowed when HDIV and PDIV are 0*/
        /*  96MHz*/
        ( 0x5F << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_1US_Pos) |
        ( 0x01 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_20NS_Pos ) |
        ( 0x05 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_CC_T_RD_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_PL_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CS_Pos ) |
        ( 0x09 << OTPC_OTPC_TIM1_REG_OTPC_TIM1_US_T_CSP_Pos ),
};
/* TIM2 settings */
static const uint32_t tim2 = {
        /* default*/
        ( 0x09 << OTPC_OTPC_TIM2_REG_OTPC_TIM2_US_T_PW_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM2_REG_OTPC_TIM2_US_T_PWI_Pos ) |
        ( 0x04 << OTPC_OTPC_TIM2_REG_OTPC_TIM2_US_T_PPR_Pos ) |
        ( 0x04 << OTPC_OTPC_TIM2_REG_OTPC_TIM2_US_T_PPS_Pos ) |
        ( 0x00 << OTPC_OTPC_TIM2_REG_OTPC_TIM2_US_T_VDS_Pos ) |
        ( 0x04 << OTPC_OTPC_TIM2_REG_OTPC_TIM2_US_T_PPH_Pos ) |
        ( 0x01 << OTPC_OTPC_TIM2_REG_OTPC_TIM2_US_T_SAS_Pos ) |
        ( 0x01 << OTPC_OTPC_TIM2_REG_OTPC_TIM2_US_ADD_CC_EN_Pos )
};


/*
 * Forward declarations
 */

/*
 * Inline helpers
 */

/*
 * Assertion macros
 */

/*
 * Make sure that the OTP clock is enabled
 */
#define ASSERT_WARNING_OTP_CLK_ENABLED \
        ASSERT_WARNING(CRG_TOP->CLK_AMBA_REG & REG_MSK(CRG_TOP, CLK_AMBA_REG, OTP_ENABLE))

/*
 * Make sure that the cell address is valid
 */
#define ASSERT_CELL_OFFSET_VALID(off) \
        ASSERT_WARNING(off < HW_OTP_CELL_NUM)

/*
 * Function definitions
 */

__RETAINED_CODE HW_OTPC_SYS_CLK_FREQ hw_otpc_convert_sys_clk_mhz(uint32_t clk_freq)
{
        switch (clk_freq) {
        case 2:
                return  HW_OTPC_SYS_CLK_FREQ_2MHz;
        case 4:
                return  HW_OTPC_SYS_CLK_FREQ_4MHz;
        case 8:
                return  HW_OTPC_SYS_CLK_FREQ_8MHz;
        case 16:
                return  HW_OTPC_SYS_CLK_FREQ_16MHz;
        case 32:
                return  HW_OTPC_SYS_CLK_FREQ_32MHz;
        case 96:
                return  HW_OTPC_SYS_CLK_FREQ_96MHz;
        default:
                /* Invalid frequency */
                return HW_OTPC_SYS_CLK_FREQ_INVALID_VALUE;
        }
}

__RETAINED_CODE void hw_otpc_disable(void)
{
        /*
         * Enable OTPC clock
         */
        hw_otpc_init();

        /*
         * set OTPC to stand-by mode
         */
        HW_OTPC_REG_SETF(MODE, MODE, HW_OTPC_MODE_DSTBY);

        hw_otpc_wait_mode_change();

        /*
         * Disable OTPC clock
         */
        hw_otpc_close();
}

bool hw_otpc_is_valid_speed(HW_OTPC_SYS_CLK_FREQ clk_speed, uint8_t clk_type)
{
        const cpu_clk_t sys_clk = (cpu_clk_t) clk_type;
        switch (sys_clk) {
        case cpuclk_2M:
                return  (clk_speed == HW_OTPC_SYS_CLK_FREQ_2MHz);
        case cpuclk_4M:
                return  (clk_speed == HW_OTPC_SYS_CLK_FREQ_4MHz);
        case cpuclk_8M:
                return  (clk_speed == HW_OTPC_SYS_CLK_FREQ_8MHz);
        case cpuclk_16M:
                return  (clk_speed == HW_OTPC_SYS_CLK_FREQ_16MHz);
        case cpuclk_32M:
                return  (clk_speed == HW_OTPC_SYS_CLK_FREQ_32MHz);
        case cpuclk_96M:
                return  (clk_speed == HW_OTPC_SYS_CLK_FREQ_96MHz);
        default:
                /* Invalid frequency */
                return false;
        }
}

HW_OTPC_ERROR_CODE hw_otpc_set_speed(HW_OTPC_SYS_CLK_FREQ clk_speed)
{
        /* Check if otpc is enabled */
        if (!hw_otpc_is_active()) {
                return HW_OTPC_ERROR_OTPC_DISABLED;
        }

        /* Check if clk_speed is valid */
        if (clk_speed >= HW_OTPC_SYS_CLK_FREQ_INVALID_VALUE) {
                return HW_OTPC_ERROR_INVALID_FREQ;
        }

        /*
         * Set access speed
         */
        OTPC->OTPC_TIM1_REG = tim1[clk_speed];
        OTPC->OTPC_TIM2_REG = tim2;

        return(HW_OTPC_ERROR_NO_ERROR);
}

bool hw_otpc_word_prog_and_verify(uint32_t wdata, uint32_t cell_offset)
{

        ASSERT_CELL_OFFSET_VALID(cell_offset);

        ASSERT_WARNING_OTP_CLK_ENABLED;

        hw_otpc_word_prog( wdata, cell_offset );

        hw_otpc_enter_mode(HW_OTPC_MODE_PVFY);
        if (wdata != *(uint32_t *)(MEMORY_OTP_BASE + 4 * cell_offset)) {
                return false;
        }

        hw_otpc_enter_mode(HW_OTPC_MODE_RINI);
        if (wdata != *(uint32_t *)(MEMORY_OTP_BASE + 4 * cell_offset)) {
                return false;
        }

        return true;
}

uint32_t hw_otpc_word_read( uint32_t cell_offset )
{
        ASSERT_CELL_OFFSET_VALID(cell_offset);

        ASSERT_WARNING_OTP_CLK_ENABLED;

        hw_otpc_enter_mode(HW_OTPC_MODE_READ);
        return *(uint32_t *)(MEMORY_OTP_BASE + 4 * cell_offset) ;
}

void hw_otpc_prog(uint32_t *p_data, uint32_t cell_offset, uint32_t num_of_words)
{
        uint32_t i;

        ASSERT_WARNING_OTP_CLK_ENABLED;
        ASSERT_CELL_OFFSET_VALID(cell_offset + num_of_words - 1);

        hw_otpc_enter_mode(HW_OTPC_MODE_PROG);

        for (i = 0; i < num_of_words; i++) {
                OTPC->OTPC_PWORD_REG = *p_data++;
                OTPC->OTPC_PADDR_REG = cell_offset++;
                hw_otpc_wait_while_programming_buffer_is_full();
        }
        hw_otpc_wait_while_busy_programming();
}

static bool hw_otpc_read_verif(uint32_t *w_data, uint32_t cell_offset, uint32_t num_of_words, HW_OTPC_MODE mode)
{
        uint32_t i;

        ASSERT_WARNING_OTP_CLK_ENABLED;

        hw_otpc_enter_mode(mode);

        for (i = 0; i < num_of_words; i++) {
                if (*w_data != *(uint32_t *)(MEMORY_OTP_BASE + 4 * cell_offset)) {
                        return false;
                }
                cell_offset++;
                w_data++;
        }
        return true;
}

bool hw_otpc_prog_and_verify(uint32_t *p_data, uint32_t cell_offset, uint32_t num_of_words)
{
        ASSERT_WARNING_OTP_CLK_ENABLED;

        hw_otpc_prog(p_data, cell_offset, num_of_words);

        if (hw_otpc_read_verif(p_data, cell_offset, num_of_words, HW_OTPC_MODE_PVFY) == false) {
                return false;
        }

        if (hw_otpc_read_verif(p_data, cell_offset, num_of_words, HW_OTPC_MODE_RINI) == false) {
                return false;
        }

        hw_otpc_enter_mode(HW_OTPC_MODE_PROG);
        return true;
}


void hw_otpc_read(uint32_t *p_data, uint32_t cell_offset, uint32_t num_of_words)
{
        uint32_t i;

        ASSERT_WARNING_OTP_CLK_ENABLED;

        ASSERT_CELL_OFFSET_VALID(cell_offset + num_of_words - 1);

        hw_otpc_enter_mode(HW_OTPC_MODE_READ);

        for (i = 0; i < num_of_words; i++) {
                *p_data = *(uint32_t *)(MEMORY_OTP_BASE + 4 * cell_offset) ;
                p_data++;
                cell_offset++;
        }
}

uint32_t hw_otpc_address_to_cell_offset(uint32_t address)
{
        /* Check if address is valid OTP address */
        ASSERT_ERROR( IS_OTP_ADDRESS(address) );
        /* Check if address is at beginning of OTP memory cell */
        ASSERT_WARNING(!(address % 4));

        if (address < MEMORY_OTP_BASE_P) {
                return (address - MEMORY_OTP_BASE) / 4;
        } else {
                return (address - MEMORY_OTP_BASE_P) / 4;
        }
}

bool hw_otpc_is_aes_key_revoked(uint8_t key_entry)
{
        uint32_t otp_key_offset = (MEMORY_OTP_USER_DATA_KEYS_INDEX_START / 4) + key_entry;

        return hw_otpc_word_read(otp_key_offset) ? false : true;
}

uint32_t hw_otpc_get_aes_key_address(uint8_t key_entry)
{
        uint32_t key_start_addr = MEMORY_OTP_BASE_P + MEMORY_OTP_USER_DATA_KEYS_PAYLOAD_START;

        ASSERT_WARNING(key_entry < HW_OTP_MAX_PAYLOAD_ENTRIES);

        if ((key_entry >= HW_OTP_MAX_PAYLOAD_ENTRIES) || hw_otpc_is_aes_key_revoked(key_entry)) {
                return 0;
        }

        return (key_start_addr + ((uint32_t) key_entry * HW_OTP_USER_DATA_KEY_SIZE));
}

#endif /* dg_configUSE_HW_OTPC */
/**
 * \}
 * \}
 * \}
 */
