/**
 ****************************************************************************************
 *
 * @file Xtal_TRIM.c
 *
 * @brief Xtal trim for DA1469x devices.
 *
 * Copyright (C) 2015-2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */


/**
 * inputs:
 *      auto_trim(HW_GPIO_PORT port, HW_GPIO_PIN pin)
 *      Xtal = 32MHz
 *      TRIM-limits are dependent of the XTAL
 *      port_number: input of the 300 msec signal for XAL-calibration
 *              e.g. 0 = P0_0-P0_31 or P1_0-P1_22 (00-31 or 100-122)
 * outputs:
 *      return (-1)     // -1 = square pulse outside boundaries
 *      return (-2)     // -2 = no square pulse detected
 *      return (-3)     // -3 = failed to write otp value
 *      return (-4)     // -4 = wrong input of port_number
 *      return (-5)     // -5 = wrong input of XTAL_select
 *      return (TRIM)   // TRIM-value is returned
 */

#include <stdio.h>
#include <hw_gpio.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "Xtal_TRIM.h"

/**
 * Variables
 */
volatile static uint32_t       Trim_Min;                // Trimming value minimum boundary
volatile static uint32_t       Trim_Max;                // Trimming value maximum boundary
volatile static uint32_t       Ideal_Cnt;               // Ideal value of XTAL
static bool                    PulseError = false;      // PusleError flag is set to true
                                                        // when no external pulses detected
volatile static uint32_t       Actual_Trimming_Value;   // The trimming value applied

/**
 * Debug parameters
 */
#if AUTO_XTAL_TEST_DBG_EN

#endif


/**
 *  @brief delay routine for 32M XTAL
 *
 *  @param[in] dd        the desired delay in ms
 */
__STATIC_FORCEINLINE void delay_ms(uint32_t dd)
{
        /* Calculate the delay loops for 32MHz XTAL */
        dd *= 2;
        dd *= DELAY_1MSEC;

        for (uint32_t  j = 1; j <= dd; j++) {
                __NOP();
                __NOP();
        }
}

/**
*  @brief Checking the boundaries for the trim_value and writes it in CLK_FREQ_TRIM_REG
*
*  @param[in] trim_value        the trim value to check and apply
*
*/
void Set_Trim_Value(uint32_t trim_value)
{
        if ((trim_value < Trim_Min) && (trim_value != 0)) {
                trim_value = Trim_Min;
        }

        if (trim_value > Trim_Max) {
                trim_value = Trim_Max;
        }

        /* Write the trim value to the register */
        REG_SETF(CRG_XTAL, CLK_FREQ_TRIM_REG, XTAL32M_TRIM, trim_value);

        /* delay for 2msec */
        delay_ms(2);
}

/**
 * @brief Measures the high duration of an externally applied square pulse in system ticks.
 *
 * @param[in] port_number    GPIO where the square pulse is applied on.
 *
 * @return   Zero if no external pulse is detected on the given GPIO.
 *           Otherwise the pulse high duration in system ticks is returned.
 */
long Clock_Read(HW_GPIO_PORT port, HW_GPIO_PIN pin)
{
        int32_t cnt_output = 0;
        uint32_t datareg;
        volatile uint32_t tick_counter = 0;

        switch (port)
        {
        case 0:
                datareg = (uint32_t)(&(GPIO->P0_DATA_REG));
                break;
        case 1:
                datareg = (uint32_t)(&(GPIO->P1_DATA_REG));
                break;
        default:
                return 0;
        }

        /* during counting, no interrupts should appear */
        GLOBAL_INT_DISABLE();                                               // disable interrupts

        /* configure systick timer */
        REG_SETF(SysTick, LOAD, RELOAD, 0xFFFFFF);
        REG_SETF(SysTick, VAL, CURRENT, 0);
        REG_SETF(SysTick, CTRL, CLKSOURCE, 1);          // use processor-clock

        tick_counter = MEASURE_PULSE(datareg, (1 << pin));
        REG_SETF(SysTick, CTRL, ENABLE, 0);                       // stop systick timer

        GLOBAL_INT_RESTORE();                           // enable interrupts

        cnt_output = 0xFFFFFF - tick_counter;

        PulseError = false;
        if (cnt_output == 0xFFFFFF) {
                PulseError = true;
        }

        return cnt_output;

}

/**
 * @brief calculate the new TRIM value via linear line algorithm
 *
 * @param[in]   C       The counter
 * @param[in]   Cmin    The counter min barrier
 * @param[in]   Cmax    The counter max barrier
 * @param[in]   Tmin    The Trim min barrier
 * @param[in]   Tmax    The Trim max barrier
 *
 * @return   The new Trim value calculated via linear line alogrithm
 *
 */
int linearization(int C, int Cmin, int Cmax, int Tmin, int Tmax)
{
        /* C = counter at 9.6M (ideal value) */
        return (int)( Tmax - ( ((C - Cmin) * (Tmax - Tmin)) / (Cmax - Cmin) ) );
}

/* main function is start of auto-calibration */
int16_t auto_trim(HW_GPIO_PORT port, HW_GPIO_PIN pin)
{
        int32_t trim_low;                               // left side value e.g. 350
        int32_t trim_hi;                                // right side value e.g 500
        int32_t c_max, c_min;
        int32_t trim_next;
        int8_t loop;                                    // loop 1 ... MAX_LOOPS (max: 10)
        int32_t pulse_duration_in_sys_ticks;            // temporary value
        int32_t pulse_duration_in_sys_ticks_at_350;     // start trimming value

        Ideal_Cnt = XTAL32M - 4 + TEMP_OFFSET;
        Trim_Min = MIN_XTAL_TRIM_VAL_BORDER;
        Trim_Max = MAX_XTAL_TRIM_VAL_BORDER;

        /* configure the selected GPIO input for the 300ms pulse */
        hw_gpio_set_pin_function(port, pin, HW_GPIO_MODE_INPUT_PULLUP, HW_GPIO_FUNC_GPIO);
        hw_gpio_pad_latch_enable(port, pin);

        /* select to  start with middle range value */
        trim_next = MIDDLE_XTAL_TRIM_VAL_BORDER;

        /* 0e - Setting_Trim & temp = Clock_Read() */
        Set_Trim_Value(trim_next);

        /* 1e - Clock_Read */
        pulse_duration_in_sys_ticks = Clock_Read(port, pin);
        pulse_duration_in_sys_ticks_at_350 = pulse_duration_in_sys_ticks;

        /* 2e - set Trim at Tmax or Tmin */
        if (pulse_duration_in_sys_ticks > XTAL32M) {
                trim_next = MAX_XTAL_TRIM_VAL_BORDER;
        }
        else {
                trim_next = MIN_XTAL_TRIM_VAL_BORDER;
        }

        Set_Trim_Value(trim_next);

        /* 3e - Clock Read at Trim */
        pulse_duration_in_sys_ticks = Clock_Read(port, pin); // at Tmin (at C_max) or Tmax (at C_min)

        loop = 0;

        do
        {
                loop++;

                /* 4e if abs(temp - C_ideal) <= in spec => break */
                if ((pulse_duration_in_sys_ticks > XTAL32M) && (pulse_duration_in_sys_ticks - XTAL32M) <= PPM_1) { // XTAL32M = C_ideal (9.6M at 300ms)
                        break;  // out of while
                }

                if ((pulse_duration_in_sys_ticks < XTAL32M) && (XTAL32M - pulse_duration_in_sys_ticks) <= PPM_1) { // XTAL32M = C_ideal (9.6M at 300ms)
                        break;  // out of while
                }

                /* 5e if (temp < C_ideal) */
                if (pulse_duration_in_sys_ticks > XTAL32M)
                {
                        trim_hi = MIDDLE_XTAL_TRIM_VAL_BORDER;          // right side graph
                        trim_low = trim_next;
                        c_min = pulse_duration_in_sys_ticks_at_350;     // at 350
                        c_max = pulse_duration_in_sys_ticks;            // at < 350
                }
                else
                {
                        trim_hi = trim_next;                            // left side graph
                        trim_low = MIDDLE_XTAL_TRIM_VAL_BORDER;
                        c_min = pulse_duration_in_sys_ticks;            // at > 350
                        c_max = pulse_duration_in_sys_ticks_at_350;     // at 350
                }

                /* 6e - Trim_next = sub-linearization */
                trim_next = linearization(XTAL32M, c_min, c_max, trim_low, trim_hi);

                /* 7e - Trim = Trim_next */
                Set_Trim_Value(trim_next);

                /* 8e - pulse_duration_in_sys_ticks = Clock_Read() */
                pulse_duration_in_sys_ticks = Clock_Read(port, pin);
        }
        while (loop < MAX_LOOPS);

        /* check existence of square pulse */
        if (PulseError == true)
        {
                /* no square pulse detected, set TRIM = 0 */
                Set_Trim_Value(0);
                /* return the no square pulse detected error */
                return NO_PULSE_ERROR;
        }

        if (pulse_duration_in_sys_ticks < (Ideal_Cnt - PPM_1))
        {
                /* pulse is out of range, set TRIM = 0 */
                Set_Trim_Value(0);
                /* return the pulse out of range detected error */
                return PULSE_OUT_OF_RANGE_ERROR;
        }

        /* read TRIM register */
        Actual_Trimming_Value = REG_GETF(CRG_XTAL, CLK_FREQ_TRIM_REG, XTAL32M_TRIM);

        if (Actual_Trimming_Value <= Trim_Min && Actual_Trimming_Value > 0) {
                /* TRIM-value = 10  out of spec! */
                /* return the pulse out of range detected error */
                return PULSE_OUT_OF_RANGE_ERROR;
        }

        if (Actual_Trimming_Value >= Trim_Max) {
                /* TRIM-value = 700 out of spec! */
                /* return the pulse out of range detected error */
                return PULSE_OUT_OF_RANGE_ERROR;
        }

        /* return the actual TRIM value applied */
        return (Actual_Trimming_Value);
}

/*
 * Debug function
 */
#if AUTO_XTAL_TEST_DBG_EN
void TRIM_test (int S1, int S2) // measuring Arrays in TRIM-values
{
        volatile signed int i, j;
        static int ff[2050];

        for (i=S1; i <= S2; i++)
        {
                REG_SETF(CRG_XTAL, CLK_FREQ_TRIM_REG, XTAL32M_TRIM, i);

                ff[i] = Clock_Read(23);
                j = 9600000 - ff[i];
                __NOP();
        }

}
#endif

