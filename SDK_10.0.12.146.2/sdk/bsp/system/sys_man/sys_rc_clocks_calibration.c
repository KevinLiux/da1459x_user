/**
 ****************************************************************************************
 *
 * @file sys_rc_clocks_calibration.c
 *
 * @brief RC calibration common source file.
 *
 * Copyright (C) 2019-2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#if (defined(OS_FREERTOS))
#if (dg_config_ENABLE_RC32K_CALIBRATION || (dg_configUSE_LP_CLK == LP_CLK_RCX))
#include "hw_pdc.h"
#include "hw_rtc.h"
#include "snc_hw_gpadc.h"
#include "hw_clk.h"

#include "../adapters/src/sys_platform_devices_internal.h"

#include "sys_rc_clocks_calibration_internal.h"

/* Debugging Macros for SNC uCodes - start */
#if (RC_CLK_CALIBRATION_DEBUG == 1)
#define DBG_UCODE_TOGGLE(flag, name)                                                            \
        SENIS_wadva(da(&name##_MODE_REG), 0x300);                                               \
        SENIS_wadva(da(&name##_SET_REG), name##_PIN);                                           \
        SENIS_if((uint32_t)&(name##_SET_REG), EQ, (uint32_t)&(GPIO->P0_SET_DATA_REG)) {         \
                SENIS_wadva(da(&CRG_TOP->P0_SET_PAD_LATCH_REG), name##_PIN);                    \
                SENIS_wadva(da(&name##_RESET_REG), name##_PIN);                                 \
                SENIS_wadva(da(&CRG_TOP->P0_RESET_PAD_LATCH_REG), name##_PIN);                  \
        SENIS_else {                                                                            \
                SENIS_wadva(da(&CRG_TOP->P1_SET_PAD_LATCH_REG), name##_PIN);                    \
                SENIS_wadva(da(&name##_RESET_REG), name##_PIN);                                 \
                SENIS_wadva(da(&CRG_TOP->P1_RESET_PAD_LATCH_REG), name##_PIN);                  \
        }}

#define DBG_UCODE_SET_HIGH(flag, name)                                                          \
        SENIS_wadva(da(&name##_MODE_REG), 0x300);                                               \
        SENIS_wadva(da(&name##_SET_REG), name##_PIN);                                           \
        SENIS_if((uint32_t)&(name##_SET_REG), EQ, (uint32_t)&(GPIO->P0_SET_DATA_REG)) {         \
                SENIS_wadva(da(&CRG_TOP->P0_SET_PAD_LATCH_REG), name##_PIN);                    \
                SENIS_wadva(da(&CRG_TOP->P0_RESET_PAD_LATCH_REG), name##_PIN);                  \
        SENIS_else {                                                                            \
                SENIS_wadva(da(&CRG_TOP->P1_SET_PAD_LATCH_REG), name##_PIN);                    \
                SENIS_wadva(da(&CRG_TOP->P1_RESET_PAD_LATCH_REG), name##_PIN);                  \
        }}

#define DBG_UCODE_SET_LOW(flag, name)                                                           \
        SENIS_wadva(da(&name##_MODE_REG), 0x300);                                               \
        SENIS_wadva(da(&name##_RESET_REG), name##_PIN);                                         \
        SENIS_if((uint32_t)&(name##_SET_REG), EQ, (uint32_t)&(GPIO->P0_SET_DATA_REG)) {         \
                SENIS_wadva(da(&CRG_TOP->P0_SET_PAD_LATCH_REG), name##_PIN);                    \
                SENIS_wadva(da(&CRG_TOP->P0_RESET_PAD_LATCH_REG), name##_PIN);                  \
        SENIS_else {                                                                            \
                SENIS_wadva(da(&CRG_TOP->P1_SET_PAD_LATCH_REG), name##_PIN);                    \
                SENIS_wadva(da(&CRG_TOP->P1_RESET_PAD_LATCH_REG), name##_PIN);                  \
        }}

#else

#define DBG_UCODE_TOGGLE(flag, name)
#define DBG_UCODE_SET_HIGH(flag, name)
#define DBG_UCODE_SET_LOW(flag, name)

#endif /* RC_CLK_CALIBRATION_DEBUG */
/* Debugging Macros for SNC uCodes - end */

_SNC_RETAINED snc_cm33_mutex_t rc_mutex;

#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
_SNC_RETAINED static uint32_t rtc_pdc_evt_cnt_minute;
#endif /* (dg_configUSE_LP_CLK == LP_CLK_RCX) */

_SNC_RETAINED static uint32_t rtc_pdc_evt_cnt;

_SNC_RETAINED static struct {
#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        uint32_t rcx_temperature_lower_bound;     // the lower temperature bound used in uCode to notify CM33 if current temperature is lower than that
        uint32_t rcx_temperature_upper_bound;     // the upper temperature bound used in uCode to notify CM33 if current temperature is higher than that
        uint32_t rcx_calibration_calc;            // the calculation value after the calibration loop has finished
        uint32_t vbat_lower_bound;                // the lower VBAT bound used in uCode to notify CM33 if current VBAT value is lower than that
        uint32_t vbat_upper_bound;                // the upper VBAT bound used in uCode to notify CM33 if current VBAT value is higher than that
        uint32_t vbat_current_value;              // current VBAT value from the GPADC measurement
#endif /* #if (dg_configUSE_LP_CLK == LP_CLK_RCX */
        uint32_t rc32k_temperature_lower_bound;   // the lower temperature bound used in uCode to notify CM33 if current temperature is lower than that
        uint32_t rc32k_temperature_upper_bound;   // the upper temperature bound used in uCode to notify CM33 if current temperature is higher than that
        uint32_t temperature_current_value;       // current temperature value from the GPADC measurement
        uint32_t rc_clocks_to_calibrate;          // bitmap indicating which RC clocks to calibrate. Bits to set: RCX_DO_CALIBRATION and RC32K_DO_CALIBRATION
} sys_adc_ucode_env;

__RETAINED static uint32_t ucode_id;

#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
__RETAINED static uint16_t rcx_temperature_drift;
__RETAINED static uint16_t rcx_vbat_drift;
#endif /*(dg_configUSE_LP_CLK == LP_CLK_RCX)*/

#if dg_config_ENABLE_RC32K_CALIBRATION
__RETAINED static uint16_t rc32k_temperature_drift;
#endif /* #if dg_config_ENABLE_RC32K_CALIBRATION */

_SNC_RETAINED static uint32_t xtal32m_pdc_entry;

static void ucode_rc_clocks_init(void)
{
        snc_mutex_SNC_create(&rc_mutex);
}

static uint16_t ucode_rc_clocks_get_values(uint16_t *vbat, uint32_t *rc_clocks_to_calibrate, uint32_t *_rcx_cal_value)
{
        uint16_t bandgap_temp_sensor;

        snc_mutex_SNC_lock(&rc_mutex);
        bandgap_temp_sensor = (uint16_t)sys_adc_ucode_env.temperature_current_value;
#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        *vbat = (uint16_t)sys_adc_ucode_env.vbat_current_value;
#else
        *vbat = 0;
#endif
        *rc_clocks_to_calibrate = sys_adc_ucode_env.rc_clocks_to_calibrate;
#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        /* Save the calibration value to use it when the
         * OS_Task need to make the calculations for the RCX frequency */
        *_rcx_cal_value = sys_adc_ucode_env.rcx_calibration_calc;
#endif /* (dg_configUSE_LP_CLK == LP_CLK_RCX) */
        snc_mutex_SNC_unlock(&rc_mutex);

        return bandgap_temp_sensor;
}

uint16_t sys_rc_clocks_calibrate_get_values(uint16_t *vbat, uint32_t *rc_clocks_to_calibrate, uint32_t *_rcx_cal_value)
{
        return ucode_rc_clocks_get_values(vbat, rc_clocks_to_calibrate, _rcx_cal_value);
}
#endif /* (dg_config_ENABLE_RC32K_CALIBRATION || (dg_configUSE_LP_CLK == LP_CLK_RCX)) */

#if dg_config_ENABLE_RC32K_CALIBRATION
static void ucode_rc32k_set_bounds(uint16_t temp_bandgap_sensor)
{
        snc_mutex_SNC_lock(&rc_mutex);

        sys_adc_ucode_env.rc32k_temperature_lower_bound = temp_bandgap_sensor - rc32k_temperature_drift;
        sys_adc_ucode_env.rc32k_temperature_upper_bound = temp_bandgap_sensor + rc32k_temperature_drift;

        snc_mutex_SNC_unlock(&rc_mutex);
}

void sys_rc32k_calibrate_set_bounds(uint16_t temp_bandgap_sensor)
{
        ucode_rc32k_set_bounds(temp_bandgap_sensor);
}
#endif /* dg_config_ENABLE_RC32K_CALIBRATION */

#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
static void ucode_rcx_set_bounds(uint16_t bandgap, uint16_t vbat)
{
        snc_mutex_SNC_lock(&rc_mutex);

        sys_adc_ucode_env.rcx_temperature_lower_bound = bandgap - rcx_temperature_drift;
        sys_adc_ucode_env.rcx_temperature_upper_bound = bandgap + rcx_temperature_drift;

        sys_adc_ucode_env.vbat_lower_bound = vbat - rcx_vbat_drift;
        sys_adc_ucode_env.vbat_upper_bound = vbat + rcx_vbat_drift;

        snc_mutex_SNC_unlock(&rc_mutex);
}

void sys_rcx_calibrate_set_bounds(uint16_t bandgap, uint16_t vbat)
{
        ucode_rcx_set_bounds(bandgap, vbat);
}
#endif /* (dg_configUSE_LP_CLK == LP_CLK_RCX) */

#if (dg_config_ENABLE_RC32K_CALIBRATION || (dg_configUSE_LP_CLK == LP_CLK_RCX))
void sys_rc_clocks_calibrate_config(ad_snc_interrupt_cb _rc_clocks_calibrate_cb)
{
        ad_snc_ucode_cfg_t cfg = { 0 };
        ad_gpadc_handle_t handle;
        uint16_t bandgap_temp_sensor_val;
#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        uint16_t vbat_val;

        /* Define the RCX temperature drift boundaries */
        rcx_temperature_drift = hw_gpadc_convert_temperature_to_raw_val(TEMP_SENSOR_BANDGAP_INTERNAL.drv, 0) -
                                hw_gpadc_convert_temperature_to_raw_val(TEMP_SENSOR_BANDGAP_INTERNAL.drv, BANDGAP_TEMPERATURE_CHANGE_FOR_RCX);
        // 1 LSB = 4.88mV
        // V = 4.88mV * (ADC_raw >> 6) --> 1mV = 64/4.88 ~ 13
        rcx_vbat_drift = VBAT_VOLTAGE_DRIFT * 13;
#endif /* (dg_configUSE_LP_CLK == LP_CLK_RCX) */

        #if dg_config_ENABLE_RC32K_CALIBRATION
        /* Define the RC32K temperature drift boundaries */
        rc32k_temperature_drift = hw_gpadc_convert_temperature_to_raw_val(TEMP_SENSOR_BANDGAP_INTERNAL.drv, 0) -
                                  hw_gpadc_convert_temperature_to_raw_val(TEMP_SENSOR_BANDGAP_INTERNAL.drv, BANDGAP_TEMPERATURE_CHANGE_FOR_RC32K);
#endif /* dg_config_ENABLE_RC32K_CALIBRATION */

        /* read the current bandgap temperature sensor value */
        handle = ad_gpadc_open(&TEMP_SENSOR_BANDGAP_INTERNAL);
        ad_gpadc_read_raw(handle, &bandgap_temp_sensor_val);
        ad_gpadc_close(handle, false);

#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        /* read the current VBAT voltage value */
        handle = ad_gpadc_open(&BATTERY_LEVEL_INTERNAL);
        ad_gpadc_read_raw(handle, &vbat_val);
        ad_gpadc_close(handle, false);
#endif /* (dg_configUSE_LP_CLK == LP_CLK_RCX) */

        /* initialize the RC clocks operational resources */
        ucode_rc_clocks_init();

#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        /* Set the boundaries for the RCX */
        ucode_rcx_set_bounds(bandgap_temp_sensor_val, vbat_val);
#endif /* (dg_configUSE_LP_CLK == LP_CLK_RCX) */
#if dg_config_ENABLE_RC32K_CALIBRATION
        /* Set the boundaries for the RC32K */
        ucode_rc32k_set_bounds(bandgap_temp_sensor_val);
#endif /* dg_config_ENABLE_RC32K_CALIBRATION */
        /* Configure the PDC event and uCode priorities */

        cfg.pdc_evt_pr = AD_SNC_PDC_EVT_PR_0;
        cfg.ucode_pr = AD_SNC_UCODE_PR_1;

        /* Set the SNC-to-CM33 notification callback */
        cfg.cb = _rc_clocks_calibrate_cb;

        /* The uCode is executed on RTC-to-PDC event interrupt */
        cfg.pdc_entry = HW_PDC_LUT_ENTRY_VAL(HW_PDC_TRIG_SELECT_PERIPHERAL,
                HW_PDC_PERIPH_TRIG_ID_RTC_TIMER, HW_PDC_MASTER_SNC, HW_PDC_LUT_ENTRY_EN_PER);

        xtal32m_pdc_entry = hw_pdc_add_entry( HW_PDC_TRIGGER_FROM_MASTER( HW_PDC_MASTER_SNC,
                                                          HW_PDC_LUT_ENTRY_EN_XTAL ));
        ucode_id = ad_snc_ucode_register(&cfg,
                SNC_UCODE_CTX(ucode_rc_clocks_calibrate_notify));

        ad_snc_ucode_enable(ucode_id);

        /* Clear possible triggered events to make sure the uCode will start running
         * in case where there is already an RTC/PDC event which is not cleared */
        hw_rtc_pdc_event_clear();
}

SNC_UCODE_BLOCK_DEF(ucode_rc_clocks_calibrate_notify)
{
        SENIS_labels(pt_no_xtal_wait,
                     pt_xtal32m_set_pending,
                     pt_rcx_checks,
#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
                     pt_wait_for_rcx_calibration_to_finish,
                     pt_rcx_calibrate_start,
#endif /*(dg_configUSE_LP_CLK == LP_CLK_RCX)*/
                     pt_wait_for_running_at_xtal32m,
                     pt_return);

        // Clear the RTC PDC event
        SNC_hw_sys_clear_rtc_pdc_event();

        SENIS_inc1(da(&rtc_pdc_evt_cnt));
        SENIS_if (da(&rtc_pdc_evt_cnt), LT, dg_configRC_CAL_POLL_INT) {
                SENIS_return;
        }
        SENIS_wadva(da(&rtc_pdc_evt_cnt), 0);

#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        SENIS_inc1(da(&rtc_pdc_evt_cnt_minute));
        SENIS_if(da(&rtc_pdc_evt_cnt_minute), EQ, UNCOND_CAL_TIME_IN_SEC) {
                SENIS_wadva(da(&rtc_pdc_evt_cnt_minute), 0);
#    if dg_config_ENABLE_RC32K_CALIBRATION
                SENIS_wadva(da(&sys_adc_ucode_env.rc_clocks_to_calibrate), RCX_DO_CALIBRATION | RC32K_DO_CALIBRATION);
                DBG_UCODE_TOGGLE(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RC32K_TRIGGER);
                DBG_UCODE_TOGGLE(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RCX_TRIGGER);
#    else
                SENIS_wadva(da(&sys_adc_ucode_env.rc_clocks_to_calibrate), RCX_DO_CALIBRATION);
                DBG_UCODE_TOGGLE(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RCX_TRIGGER);
#    endif /* dg_config_ENABLE_RC32K_CALIBRATION */
                SENIS_goto(l(pt_xtal32m_set_pending));
        }
#endif /*(dg_configUSE_LP_CLK == LP_CLK_RCX)*/

        _SNC_TMP_ADD(uint32_t, local_sys_adc_ucode_env, 2 * sizeof(uint32_t));

        // Initialize - Acquire the selected GPADC source device
        SNC_gpadc_open(&TEMP_SENSOR_BANDGAP_INTERNAL);
        // Get adc measurement
        SNC_gpadc_read(&TEMP_SENSOR_BANDGAP_INTERNAL, da(&local_sys_adc_ucode_env[0]));
        // De-initialize - Release the GPADC
        SNC_gpadc_close(&TEMP_SENSOR_BANDGAP_INTERNAL);

#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        // Initialize - Acquire the selected GPADC source device
        SNC_gpadc_open(&BATTERY_LEVEL_INTERNAL);
        // Get adc measurement
        SNC_gpadc_read(&BATTERY_LEVEL_INTERNAL, da(&local_sys_adc_ucode_env[1]));
        // De-initialize - Release the GPADC
        SNC_gpadc_close(&BATTERY_LEVEL_INTERNAL);
#endif /*(dg_configUSE_LP_CLK == LP_CLK_RCX) */

        SNC_MUTEX_LOCK(&rc_mutex);

        SENIS_wadad(da(&sys_adc_ucode_env.temperature_current_value), da(&local_sys_adc_ucode_env[0]));
        SENIS_wadva(da(&sys_adc_ucode_env.rc_clocks_to_calibrate), 0);
#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        SENIS_wadad(da(&sys_adc_ucode_env.vbat_current_value), da(&local_sys_adc_ucode_env[1]));
#endif /*(dg_configUSE_LP_CLK == LP_CLK_RCX) */

        _SNC_TMP_RMV(local_sys_adc_ucode_env);

#if dg_config_ENABLE_RC32K_CALIBRATION
        // Notify CM33 based on the lower and upper temperature bounds for RC32K are crossed
        SENIS_if (da(&sys_adc_ucode_env.temperature_current_value), LT, da(&sys_adc_ucode_env.rc32k_temperature_lower_bound)) {
                SENIS_wadva(da(&sys_adc_ucode_env.rc_clocks_to_calibrate), RC32K_DO_CALIBRATION);
                DBG_UCODE_TOGGLE(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RC32K_TRIGGER);
                SENIS_goto(l(pt_rcx_checks));
        }

        SENIS_if (da(&sys_adc_ucode_env.temperature_current_value), GT, da(&sys_adc_ucode_env.rc32k_temperature_upper_bound)) {
                SENIS_wadva(da(&sys_adc_ucode_env.rc_clocks_to_calibrate), RC32K_DO_CALIBRATION);
                DBG_UCODE_TOGGLE(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RC32K_TRIGGER);
                SENIS_goto(l(pt_rcx_checks));
        }
#endif /* dg_config_ENABLE_RC32K_CALIBRATION */

        SENIS_label(pt_rcx_checks);
#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        // Notify CM33 based on the lower and upper temperature bounds
        SENIS_if (da(&sys_adc_ucode_env.temperature_current_value), LT, da(&sys_adc_ucode_env.rcx_temperature_lower_bound)) {
                SENIS_xor(da(&sys_adc_ucode_env.rc_clocks_to_calibrate), RCX_DO_CALIBRATION);
                DBG_UCODE_TOGGLE(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RCX_TRIGGER);
                /* Nothing more to check, proceed with the XTAL32M preparation and M33 notification */
                SENIS_goto(l(pt_xtal32m_set_pending));
        }
        SENIS_if (da(&sys_adc_ucode_env.temperature_current_value), GT, da(&sys_adc_ucode_env.rcx_temperature_upper_bound)) {
                SENIS_xor(da(&sys_adc_ucode_env.rc_clocks_to_calibrate), RCX_DO_CALIBRATION);
                DBG_UCODE_TOGGLE(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RCX_TRIGGER);
                /* Nothing more to check, proceed with the XTAL32M preparation and M33 notification */
                SENIS_goto(l(pt_xtal32m_set_pending));
        }

        // Notify CM33 based on the lower and upper VBAT bounds
        SENIS_if (da(&sys_adc_ucode_env.vbat_current_value), LT, da(&sys_adc_ucode_env.vbat_lower_bound)) {
                SENIS_xor(da(&sys_adc_ucode_env.rc_clocks_to_calibrate), RCX_DO_CALIBRATION);
                DBG_UCODE_TOGGLE(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RCX_TRIGGER);
                /* Nothing more to check, proceed with the XTAL32M preparation and M33 notification */
                SENIS_goto(l(pt_xtal32m_set_pending));
        }
        SENIS_if (da(&sys_adc_ucode_env.vbat_current_value), GT, da(&sys_adc_ucode_env.vbat_upper_bound)) {
                SENIS_xor(da(&sys_adc_ucode_env.rc_clocks_to_calibrate), RCX_DO_CALIBRATION);
                DBG_UCODE_TOGGLE(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RCX_TRIGGER);
                /* Nothing more to check, proceed with the XTAL32M preparation and M33 notification */
                SENIS_goto(l(pt_xtal32m_set_pending));
        }
#endif /* (dg_configUSE_LP_CLK == LP_CLK_RCX) */

        /* If there is no RC-Clock to calibrate flag set, then just jump at the end
         * otherwise proceed to start the XTAL32M and wait for it to settle */
        SENIS_if (da(&sys_adc_ucode_env.rc_clocks_to_calibrate), EQ, 0) {
                SENIS_goto(l(pt_return));
        }

        SENIS_label(pt_xtal32m_set_pending);

        DBG_UCODE_SET_HIGH(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_XTAL32M_SETTLE);
        /* The SNC is triggered by the PDC entry for the RTC.
         * This RTC-PDC entry does not enable the XTAL to save power.
         * The PDC entry enabling the XTAL32M is not associated with this uCode
         * so we need to trigger it manually in the case of RCX
         * to have the DIVN coming from XTAL32M for the needed accuracy
         */

        // 1. Set PENDING the PDC entry which will turn-on the XTAL32M
        SENIS_wadva(da(&PDC->PDC_SET_PENDING_REG), xtal32m_pdc_entry);
        // 2. Then acknowledge the PDC entry to be ready for the next time
        SNC_hw_sys_pdc_acknowledge(xtal32m_pdc_entry);

        // 3. Wait for XTAL32M to settle
        SNC_hw_sys_wait_xtalm_ready();

        // 4. Switch from RC32M to XTAL32M ...
        //    If the system is running with XTAL32M or PLL96 this will change nothing
        SENIS_wadva(da(&CRG_TOP->CLK_SWITCH2XTAL_REG), 0x1);

        // 5. Finally wait for the system to start running with XTAL32M or PLL
        //    before proceeding with enabling the HW block for clocks calibration.
        SENIS_label(pt_wait_for_running_at_xtal32m);
        SENIS_rdcbi(da(&CRG_TOP->CLK_CTRL_REG), REG_POS(CRG_TOP, CLK_CTRL_REG, RUNNING_AT_XTAL32M));
        SENIS_cobr_eq(l(pt_no_xtal_wait));
        SENIS_rdcbi(da(&CRG_TOP->CLK_CTRL_REG), REG_POS(CRG_TOP, CLK_CTRL_REG, RUNNING_AT_PLL96M));
        SENIS_cobr_eq(l(pt_no_xtal_wait));
        SENIS_goto(l(pt_wait_for_running_at_xtal32m));

        SENIS_label(pt_no_xtal_wait);
        DBG_UCODE_SET_LOW(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_XTAL32M_SETTLE);

#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
        /* Proceed with RCX calibration only if the RCX calibration flag is set */
        SENIS_rdcbi(da(&sys_adc_ucode_env.rc_clocks_to_calibrate), RCX_DO_CALIBRATION_POS);
        SENIS_cobr_eq(l(pt_rcx_calibrate_start));
        SENIS_goto(l(pt_return));

        SENIS_label(pt_rcx_calibrate_start);
        DBG_UCODE_SET_HIGH(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RCX_CAL_START);
        // start RCX calibration
        SENIS_wadva(da(&ANAMISC_BIF->CLK_REF_CNT_REG), RCX_CALIBRATION_CYCLES_WUP);
        SENIS_wadva(da(&ANAMISC_BIF->CLK_REF_SEL_REG), 0xB); // DIVN as reference clock, RCX as clock input for
                                                             // calibration, start calibration

        // wait for RCX calibration to finish
        SENIS_label(pt_wait_for_rcx_calibration_to_finish);
        SENIS_rdcbi(da(&ANAMISC_BIF->CLK_REF_SEL_REG), REG_POS(ANAMISC_BIF, CLK_REF_SEL_REG, REF_CAL_START));
        SENIS_cobr_eq(l(pt_wait_for_rcx_calibration_to_finish));
        // Save the value to be used later by M33 to calculate teh RCX frequency
        SENIS_wadad(da(&sys_adc_ucode_env.rcx_calibration_calc), da(&ANAMISC_BIF->CLK_REF_VAL_REG));
        DBG_UCODE_SET_LOW(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_RCX_CAL_START);
#endif /* (dg_configUSE_LP_CLK == LP_CLK_RCX) */

        SENIS_label(pt_return);
        /* release the MUTEX */
        SNC_MUTEX_UNLOCK(&rc_mutex);

        SENIS_if (da(&sys_adc_ucode_env.rc_clocks_to_calibrate), GT, 0) {
                DBG_UCODE_TOGGLE(RC_CLK_CALIBRATION_DEBUG, RCCLKDBG_UCODE_M33_NOTIFY);
                /* If there is any RC clock flag set then notify the M33 */
                SNC_CM33_NOTIFY();
        }
}
#endif /* (dg_config_ENABLE_RC32K_CALIBRATION || (dg_configUSE_LP_CLK == LP_CLK_RCX)) */
#endif /* (defined(OS_FREERTOS))*/
