/**
 ****************************************************************************************
 *
 * @file Xtal_TRIM.h
 *
 * @brief Xtal trim API
 *
 * Copyright (C) 2015-2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef XTAL_TRIM_H_
#define XTAL_TRIM_H_

#include <hw_gpio.h>

#define AUTO_XTAL_TEST_DBG_EN           ( 0 )           // Enable/Disable debug parameters.

/*
 *  Macro definitions
 */

/* Return Status Codes */
#define PULSE_OUT_OF_RANGE_ERROR        (-1)            // Pulse found in the pulse pin assigned GPIO was out of acceptable range
#define NO_PULSE_ERROR                  (-2)            // No pulse found, or pulse > 740 ms (measure_pulse aborts)
#define WRITING_VAL_TO_OTP_ERROR        (-3)            // Failed to write value in OTP.
#define INVALID_GPIO_ERROR              (-4)            // Wrong GPIO configuration.
#define WRONG_XTAL_SOURCE_ERROR         (-5)            // Incorrect pulse detected.

/* General parameters */
#define TEMP_OFFSET                     ( 0 )           // 9.6 = 1 ppm (32M)
#define ACCURACY_STEP                   ( 9 )           // using the SYSTICK: accuracy is 9 clocks
#define DELAY_1MSEC                     ( 1777 )        // delay x * 1 msec
#define PPM_1                           ( 10 )          // 1.04 ppm (9.6M)
#define PPM_2                           ( 20 )          // 2.08 ppm (9.632M)

#define PPM_BOUNDARY                    ( 96 )          // 96 = 10 ppm (9M6) at 32 MHz

#define NB_TRIM_DATA                    ( 11 )          // max amount of iterations
#define MAX_LOOPS                       ( 10 )

/* XTAL 32M specific */
#define XTAL32M                         ( 9600000 )     // 300 msec  TRIM = 252 (ideal 32M * 0.3 = 9.6M)
#define MIN_XTAL_TRIM_VAL_BORDER        ( 10 )          // minimum TRIM value (limits 4 - 12 pF)
#define MIDDLE_XTAL_TRIM_VAL_BORDER     ( 350 )         // starting TRIM value
#define MAX_XTAL_TRIM_VAL_BORDER        ( 700 )         // maximum TRIM value

/*
 * Function Declaration
 */
__STATIC_FORCEINLINE void delay_ms(uint32_t dd);
void Set_Trim_Value(uint32_t Trim_Value);
long Clock_Read(uint8_t port, uint8_t pin);
//int Overall_calculation(uint8_t port_number);
int  linearization(int C, int Cmin, int Cmax, int Tmin, int Tmax);
int16_t auto_trim(HW_GPIO_PORT port, HW_GPIO_PIN pin);
uint32_t pulse_counter(void);                                   // counting pulses during 500 msec
uint32_t MEASURE_PULSE(int32_t datareg1, int32_t shift_bit1);   // see assembly code

#if AUTO_XTAL_TEST_DBG_EN
void TRIM_test (int S1, int S2);                                // testing
#endif

#endif /* XTAL_TRIM_H_ */

