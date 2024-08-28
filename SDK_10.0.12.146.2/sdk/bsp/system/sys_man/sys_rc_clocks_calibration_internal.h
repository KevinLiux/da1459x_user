/**
 ****************************************************************************************
 *
 * @file sys_rc_clocks_calibration_internal.h
 *
 * @brief RC clocks calibration internal header file
 *
 * Copyright (C) 2021-2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef SYS_RC_CLOCKS_CALIBRATION_INTERNAL_H_
#define SYS_RC_CLOCKS_CALIBRATION_INTERNAL_H_

#define BANDGAP_TEMPERATURE_CHANGE_FOR_RC32K             ( 3 )     // Temperature in degrees Celsius


#if (defined(OS_FREERTOS))
#if (dg_config_ENABLE_RC32K_CALIBRATION || (dg_configUSE_LP_CLK == LP_CLK_RCX))

#include "ad_gpadc.h"
#include "ad_snc.h"
#include "snc_hw_sys.h"

#define RCX_DO_CALIBRATION_POS                  0
#define RC32K_DO_CALIBRATION_POS                1

#if (dg_config_ENABLE_RC32K_CALIBRATION)
/* Bit field to trigger the RC32K Calibration task to start calibration. */
#define RC32K_DO_CALIBRATION                    ( 1 << RC32K_DO_CALIBRATION_POS )

/**
 * \brief Function that sets the upper, lower bounds for both bandgap and Vbat
 *
 * \param [in] bandgap  the value for bandgap based on which the lower and upper bounds
 *                      are defined
 */
void sys_rc32k_calibrate_set_bounds(uint16_t temp_bandgap_sensor);
#endif /* (dg_config_ENABLE_RC32K_CALIBRATION) */

#if (dg_configUSE_LP_CLK == LP_CLK_RCX)
/* Bit field to trigger the RCX Calibration task to start calibration. */
#define RCX_DO_CALIBRATION                      ( 1 << RCX_DO_CALIBRATION_POS )
/* ~1.7 msec for any subsequent calibration. */
#define RCX_CALIBRATION_CYCLES_WUP                  ( 25 )

#define VBAT_VOLTAGE_DRIFT                          ( 800 )     // in mV
#define BANDGAP_TEMPERATURE_CHANGE_FOR_RCX          ( 1 )       // in C

#define RCX_CAL_POLL_INT                            ( 1000 )    // in msec
#define UNCOND_CAL_TIME_IN_SEC                      ( 60 )      // Time in sec for triggering unconditionally RC clocks calibration

/**
 * \brief Function that sets the upper, lower bounds for both bandgap and Vbat
 *
 * \param [in] bandgap  the value for bandgap based on which the lower and upper bounds
 *                      are defined
 * \param [in] vbat     the value for Vbat based on which the lower and upper bounds
 *                      are defined
 */
void sys_rcx_calibrate_set_bounds(uint16_t bandgap, uint16_t vbat);
#endif /* (dg_configUSE_LP_CLK == LP_CLK_RCX) */

extern _SNC_RETAINED snc_cm33_mutex_t rc_mutex;
/**
 * \brief Function that configures and registers the RC clocks uCode, measures the current value of both
 *        bandgap temperature sensor and Vbat and calculates the initial upper and lower bounds.
 *
 * \param [in] _rc_clocks_calibrate_cb        a callback function that should be called
 *                                            whenever the RC clocks uCode-Block notifies CM33
 */
void sys_rc_clocks_calibrate_config(ad_snc_interrupt_cb _rc_clocks_calibrate_cb);

/**
 * \brief Function that returns the GPADC measurements and the flags for the RC clocks to calibrate
 *
 * \param [out] vbat                       the GPADC raw value for the VBAT measurement
 * \param [out] rc_clocks_to_calibrate     the flags for which RC clock to calibrate (indicated by
 *                                         RCX_DO_CALIBRATION and RC32K_DO_CALIBRATION flags)
 * \param [out] _rcx_cal_value             the RCX calibration count value to be used for the RCX calculations
 *
 * \return uint16_t     the GPADC raw value for the bandgap temperature sensor measurement
 */
uint16_t sys_rc_clocks_calibrate_get_values(uint16_t *vbat, uint32_t *rc_clocks_to_calibrate, uint32_t *_rcx_cal_value);

/**
 * \brief uCode-Block that reads measurements from both bandgap area temperatures and VBAT.
 *        For the RC32K only the temperature is used.
 *        For the RCX, both temperature and VBAT are used.
 *        If any measurement (temperature or VBAT) exceeds the corresponding boundaries,
 *        then the SNC starts starts the XTAL32M, in the case where the RCX is the LP clock
 *        also runs the RCX calibration HW block and then notifies M33 to process the data
 *        accordingly and calculate the RCX frequency and/or trim the RC32K and calculate
 *        its frequency.
 *
 */
SNC_UCODE_BLOCK_DECL(ucode_rc_clocks_calibrate_notify);

#endif /* (dg_config_ENABLE_RC32K_CALIBRATION || (dg_configUSE_LP_CLK == LP_CLK_RCX)) */
#endif /* defined(OS_FREERTOS)*/


#endif /* SYS_RC_CLOCKS_CALIBRATION_INTERNAL_H_ */
