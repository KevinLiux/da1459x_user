/**
 ****************************************************************************************
 *
 * @file main.c
 *
 * @brief WSP weightscale application
 *
 * Copyright (C) 2015-2019 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */
#include <string.h>
#include <stdbool.h>
#include "osal.h"
#include "resmgmt.h"
#include "ad_ble.h"
#include "ad_nvms.h"
#include "ble_mgr.h"
#include "sys_clock_mgr.h"
#include "sys_power_mgr.h"
#include "sys_watchdog.h"
#include "hw_gpio.h"
#include "hw_pdc.h"
#include "hw_wkup.h"
#include "wsp_weightscale_config.h"
#include "console.h"
#include "cli.h"

/* Task priorities */
#define mainBLE_WSP_WEIGHTSCALE_TASK_PRIORITY              ( OS_TASK_PRIORITY_NORMAL )

#if dg_configUSE_WDOG
__RETAINED_RW int8_t idle_task_wdog_id = -1;
#endif

/*
 * Perform any application specific hardware configuration.  The clocks,
 * memory, etc. are configured before main() is called.
 */
static void prvSetupHardware( void );
/*
 * Task functions .
 */
void wsp_weightscale_task(void *params);

void measurement_cb(void);

static void wkup_gpio_p0_interrupt_cb()
{
        /* Clear interrupt if the source is the UART CTS pin (P0_7) */
        if (hw_wkup_get_status(SER1_CTS_PORT) & (1 << SER1_CTS_PIN)) {
                console_wkup_handler();
                hw_wkup_clear_status(HW_GPIO_PORT_0, (1 << SER1_CTS_PIN));
        }
}

static void init_wkup_gpio_p0(void)
{
        /* Enable wake up from non debounced GPIO */
        hw_wkup_gpio_configure_pin(SER1_CTS_PORT, SER1_CTS_PIN, 1, HW_WKUP_PIN_STATE_LOW);
        /* Register callback for (non-debounced) GPIO port 0 interrupt */
        hw_wkup_register_gpio_p0_interrupt(wkup_gpio_p0_interrupt_cb, 1);

        /* Setup PDC entry to wake up from UART CTS pin */
        {
                uint32 idx = hw_pdc_add_entry(HW_PDC_LUT_ENTRY_VAL(
                                                        HW_PDC_TRIG_SELECT_P0_GPIO, SER1_CTS_PIN,
                                                        HW_PDC_MASTER_CM33,
                                                        HW_PDC_LUT_ENTRY_EN_XTAL));
                hw_pdc_set_pending(idx);
                hw_pdc_acknowledge(idx);
        }
}

static void wkup_handler()
{

        if (hw_gpio_get_pin_status(CFG_TRIGGER_PERFORM_NOTIF_ACTION_GPIO_PORT,
                                        CFG_TRIGGER_PERFORM_NOTIF_ACTION_GPIO_PIN) == false) {
                measurement_cb();
        }

        hw_wkup_reset_interrupt();
}

static void init_wakeup(void)
{
        hw_wkup_init(NULL);

        /* common button configuration for DA1468X/DA1469X */
        hw_wkup_configure_pin(CFG_TRIGGER_PERFORM_NOTIF_ACTION_GPIO_PORT,
                                CFG_TRIGGER_PERFORM_NOTIF_ACTION_GPIO_PIN, 1, HW_WKUP_PIN_STATE_LOW);

        hw_wkup_set_debounce_time(10);

        hw_wkup_register_key_interrupt(wkup_handler, 1);
        hw_wkup_enable_irq();
}

/**
 * @brief System Initialization and creation of the BLE task
 */
static void system_init( void *pvParameters )
{
        OS_TASK handle;


#if defined CONFIG_RETARGET
        extern void retarget_init(void);
#endif

        /* Prepare clocks. Note: cm_cpu_clk_set() and cm_sys_clk_set() can be called only from a
         * task since they will suspend the task until the XTAL has settled and, maybe, the PLL
         * is locked.
         */
        cm_sys_clk_init(sysclk_XTAL32M);
        cm_apb_set_clock_divider(apb_div1);
        cm_ahb_set_clock_divider(ahb_div1);
        cm_lp_clk_init();

        /*
         * Initialize platform watchdog
         */
        sys_watchdog_init();

#if dg_configUSE_WDOG
        // Register the Idle task first.
        idle_task_wdog_id = sys_watchdog_register(false);
        ASSERT_WARNING(idle_task_wdog_id != -1);
        sys_watchdog_configure_idle_id(idle_task_wdog_id);
#endif


        /* Prepare the hardware to run this demo. */
        prvSetupHardware();

#if defined CONFIG_RETARGET
        retarget_init();
#endif

        /* Initialize wakeup pins used in demo */
        init_wakeup();

        /* Initialize wakeup from non-debounced IRQ used in demo */
        init_wkup_gpio_p0();

        /* Set the desired sleep mode. */
        pm_set_wakeup_mode(true);
        pm_sleep_mode_set(pm_mode_extended_sleep);

        /* init resources */
        resource_init();

        /* Initialize cli framework */
        cli_init();

        /* Initialize BLE Manager */
        ble_mgr_init();

        /* Start the WSP Weightscale application task. */
        OS_TASK_CREATE("WSP Weightscale",                    /* The text name assigned to the task, for
                                                                debug only; not used by the kernel. */
                       wsp_weightscale_task,                 /* The function that implements the task. */
                       NULL,                                 /* The parameter passed to the task. */
                       1280,                                 /* The number of bytes to allocate to the
                                                                stack of the task. */
                       mainBLE_WSP_WEIGHTSCALE_TASK_PRIORITY,/* The priority assigned to the task. */
                       handle);                              /* The task handle. */
        OS_ASSERT(handle);

        /* the work of the SysInit task is done */
        OS_TASK_DELETE(OS_GET_CURRENT_TASK());
}
/*-----------------------------------------------------------*/

/**
 * @brief Basic initialization and creation of the system initialization task.
 */
int main( void )
{
        OS_TASK handle;
        OS_BASE_TYPE status;


        /* Start SysInit task. */
        status = OS_TASK_CREATE("SysInit",                /* The text name assigned to the task, for
                                                             debug only; not used by the kernel. */
                                system_init,              /* The System Initialization task. */
                                ( void * ) 0,             /* The parameter passed to the task. */
                                1200,                     /* The number of bytes to allocate to the
                                                             stack of the task. */
                                OS_TASK_PRIORITY_HIGHEST, /* The priority assigned to the task. */
                                handle );                 /* The task handle */
        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);

        /* Start the tasks and timer running. */
        vTaskStartScheduler();

        /* If all is well, the scheduler will now be running, and the following
        line will never be reached.  If the following line does execute, then
        there was insufficient FreeRTOS heap memory available for the idle and/or
        timer tasks     to be created.  See the memory management section on the
        FreeRTOS web site for more details. */
        for ( ;; );
}

/**
 * \brief Initialize the peripherals domain after power-up.
 *
 */
static void periph_init(void)
{
        /* common button configuration for DA1468X/DA1469X */
        hw_gpio_configure_pin(CFG_TRIGGER_PERFORM_NOTIF_ACTION_GPIO_PORT,
                                                   CFG_TRIGGER_PERFORM_NOTIF_ACTION_GPIO_PIN,
                                                   HW_GPIO_MODE_INPUT_PULLUP, HW_GPIO_FUNC_GPIO, 1);
        HW_GPIO_PAD_LATCH_ENABLE(CFG_TRIGGER_PERFORM_NOTIF_ACTION_GPIO);
        HW_GPIO_PAD_LATCH_DISABLE(CFG_TRIGGER_PERFORM_NOTIF_ACTION_GPIO);
}

static void prvSetupHardware( void )
{

        /* Init hardware */
        pm_system_init(periph_init);

}

/**
 * @brief Malloc fail hook
 */
void vApplicationMallocFailedHook( void )
{
        /* vApplicationMallocFailedHook() will only be called if
        configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
        function that will get called if a call to pvPortMalloc() fails.
        pvPortMalloc() is called internally by the kernel whenever a task, queue,
        timer or semaphore is created.  It is also called by various parts of the
        demo application.  If heap_1.c or heap_2.c are used, then the size of the
        heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
        FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
        to query the size of free heap space that remains (although it does not
        provide information on how the remaining heap might be fragmented). */
        ASSERT_ERROR(0);
}

/**
 * @brief Application idle task hook
 */
void vApplicationIdleHook( void )
{
        /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
           to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
           task. It is essential that code added to this hook function never attempts
           to block in any way (for example, call xQueueReceive() with a block time
           specified, or call vTaskDelay()).  If the application makes use of the
           vTaskDelete() API function (as this demo application does) then it is also
           important that vApplicationIdleHook() is permitted to return to its calling
           function, because it is the responsibility of the idle task to clean up
           memory allocated by the kernel to any task that has since been deleted. */

#if dg_configUSE_WDOG
        sys_watchdog_notify(idle_task_wdog_id);
#endif
}

/**
 * @brief Application stack overflow hook
 */
void vApplicationStackOverflowHook( OS_TASK pxTask, char *pcTaskName )
{
        ( void ) pcTaskName;
        ( void ) pxTask;

        /* Run time stack overflow checking is performed if
        configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
        function is called if a stack overflow is detected. */
        ASSERT_ERROR(0);
}

/**
 * @brief Application tick hook
 */
void vApplicationTickHook( void )
{
}
