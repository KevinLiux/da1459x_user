/**
 ****************************************************************************************
 *
 * @file assertions.c
 *
 * @brief Assertion functions implementation.
 *
 * Copyright (C) 2021 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include "sdk_defs.h"
#include "hw_watchdog.h"
#ifdef OS_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif
#include "hw_sys.h"


#if (dg_configIMAGE_SETUP == DEVELOPMENT_MODE)

#define MTB_MASTER_REG                  ((uint32_t *) (0xE0043004))
#define MTB_MASTER_DISABLE_MSK          (0x00000009)

/**
 * When MTB is enabled, stop the tracing to prevent polluting the MTB
 * buffer with the while(1) in the assert_warning_uninit(), config_assert()
 * and assert_warning()
 */
__STATIC_FORCEINLINE void disable_tracing(void)
{
#if dg_configENABLE_MTB
        /* Disable MTB */
        *MTB_MASTER_REG = MTB_MASTER_DISABLE_MSK;
#endif /* dg_configENABLE_MTB */
}

/**
 * The assert_warning() is used from anywhere in the code and is placed in
 * retention RAM to be safely called in all cases
 */
__RETAINED_CODE static void assert_warning(void)
{
        __disable_irq();
        disable_tracing();
        hw_watchdog_freeze();
        if (EXCEPTION_DEBUG == 1) {
                hw_sys_assert_trigger_gpio();
        }
        do {} while (1);
}

/**
 * The assert_warning_uninit() is used only during boot in SystemInitPre()
 * while the RAM is not initialized yet, thus is selected to run from FLASH.
 */
static void assert_warning_uninit(void)
{
        __disable_irq();
        disable_tracing();
        GPREG->SET_FREEZE_REG = GPREG_SET_FREEZE_REG_FRZ_SYS_WDOG_Msk;
        do {} while (1);
}

#ifdef OS_FREERTOS
__RETAINED_CODE void config_assert(void)
{
        taskDISABLE_INTERRUPTS();
        disable_tracing();
        hw_watchdog_freeze();
        do {} while (1);
}
#endif /* OS_FREERTOS */

#else /* dg_configIMAGE_SETUP == DEVELOPMENT_MODE */

__RETAINED_CODE static void assert_error(void)
{
                __disable_irq();
                __BKPT(2);
}

static void assert_error_uninit(void)
{
                __disable_irq();
                __BKPT(2);
}

#endif /* dg_configIMAGE_SETUP == DEVELOPMENT_MODE */


/* Pointers to assertion functions to use
 * In the SystemInitPre these will be initialized to
 * assert_warning_uninit() and assert_error_uninit() (PRODUCTION_MODE)
 * Then the RAM will be initialized and they will take the normal assignment
 * below pointing to assert_warning() and assert_error() (PRODUCTION_MODE)
 */
#if (dg_configIMAGE_SETUP == DEVELOPMENT_MODE)
__RETAINED_RW assertion_func_t assert_warning_func = assert_warning;
__RETAINED_RW assertion_func_t assert_error_func = assert_warning;
#else
__RETAINED_RW assertion_func_t assert_error_func = assert_error;
#endif /* (dg_configIMAGE_SETUP == DEVELOPMENT_MODE) */


void assertion_functions_set_to_init(void)
{
#if (dg_configIMAGE_SETUP == DEVELOPMENT_MODE)
        assert_warning_func = assert_warning;
        assert_error_func = assert_warning;
#else
        assert_error_func = assert_error;
#endif
}

void assertion_functions_set_to_uninit(void)
{
#if (dg_configIMAGE_SETUP == DEVELOPMENT_MODE)
        assert_warning_func = assert_warning_uninit;
        assert_error_func = assert_warning_uninit;
#else
        assert_error_func = assert_error_uninit;
#endif
}
