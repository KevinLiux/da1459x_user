/**
 ****************************************************************************************
 *
 * @file sys_usb_da1469x.c
 *
 * @brief System USB
 *
 * Copyright (C) 2018-2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

/**
 \addtogroup BSP
 \{
 \addtogroup SYSTEM
 \{
 \addtogroup SYS_USB
 \{
 */
#if (dg_configUSE_SYS_USB == 1)

#include "sys_usb.h"
#include "sys_usb_da1469x_internal.h"
#include <osal.h>
#include <sys_power_mgr.h>
#include <sys_clock_mgr.h>
#include <hw_gpio.h>
#include <../adapters/src/ad_pmu_internal.h>
#ifdef OS_FREERTOS
#include "resmgmt.h"
#endif
/************************************** Private definitions ***************************************/

#define SYS_USB_20ms_SAFE_READOUT_MARGIN           20       /*  20ms */

#define USB_DEFAULT_CLK                            (sysclk_XTAL32M)

/************************************** Forward Declarations **************************************/

static void sys_usb_task(void* pvParameters);
static void sys_usb_process_attach(void);
static void sys_usb_process_detach(void);
static void sys_usb_process_vbus_event(void);

#if (dg_configUSE_USB_ENUMERATION == 1)
static void sys_usb_assert_usb_data_pin_conf(void);
static void sys_usb_idle_on_suspend(bool set_idle);
extern void set_sdk_callbacks_1469x();
extern void set_emusb_1469x_driver();

#if (dg_configUSB_DMA_SUPPORT == 1)

__RETAINED static sys_usb_driver_conf_t usb_cfg;
__RETAINED static sys_usb_driver_conf_t usb_cfg_bkup;

#endif /* dg_configUSB_DMA_SUPPORT */
#endif /* (dg_configUSE_USB_ENUMERATION == 1) */

/************************************** OS handlers ***********************************************/

__RETAINED static OS_TASK sys_usb_task_h;

/************************************** Housekeeping variables ************************************/

__RETAINED static bool sys_usb_is_process_attach_completed;
#if (dg_configUSE_USB_ENUMERATION == 1)
__RETAINED static bool sys_usb_is_pll96_activated;
__RETAINED_RW static bool sys_usb_is_suspended = false;
__RETAINED_RW static bool sys_usb_remote_wakeup_ready = false;
#endif /* (dg_configUSE_USB_ENUMERATION == 1) */

/************************************** Private types *********************************************/

/*
 * List of exchanged messages between tasks / ISR's
 */
typedef enum {
        SYS_USB_TASK_MSG_VBUS_UNKNOWN      = 0,           /* VBUS state is unknown */
        SYS_USB_TASK_MSG_VBUS_RISE         = (1 << 1),    /* VBUS is attached  */
        SYS_USB_TASK_MSG_VBUS_FALL         = (1 << 2),    /* VBUS is detached */
        SYS_USB_TASK_MSG_USB_RESET         = (1 << 3),    /* USB RESET detected */
        SYS_USB_TASK_MSG_USB_SUSPEND       = (1 << 4),    /* USB SUSPEND detected */
        SYS_USB_TASK_MSG_USB_RESUME        = (1 << 5),    /* USB RESUME detected */
        SYS_USB_TASK_MSG_SYS_CLOCK_RESET   = (1 << 6),    /* restore the system clock*/
        SYS_USB_TASK_MSG_SYS_CLOCK_PLL     = (1 << 7),    /* set the system clock to PLL96*/
        SYS_USB_TASK_MSG_SYS_REMOTE_WAKEUP = (1 << 8),    /* send remote wakeup from USB device */
} SYS_USB_TASK_MSG_STAT;

/************************************** ISR callbacks *********************************************/

static void sys_usb_vbus_isr_cb(HW_USB_VBUS_IRQ_STAT status)
{
        if (status & HW_USB_VBUS_IRQ_STAT_RISE) {
                OS_TASK_NOTIFY_FROM_ISR(sys_usb_task_h, SYS_USB_TASK_MSG_VBUS_RISE, OS_NOTIFY_SET_BITS);
        } else if (status & HW_USB_VBUS_IRQ_STAT_FALL) {
                OS_TASK_NOTIFY_FROM_ISR(sys_usb_task_h, SYS_USB_TASK_MSG_VBUS_FALL, OS_NOTIFY_SET_BITS);
        }
}

__UNUSED static void sys_usb_usb_isr_cb(uint32_t status)
{
        hw_usb_interrupt_handler(status);
}

/************************************** Processing Tasks ******************************************/

#ifdef OS_FREERTOS
# if (dg_configUSB_DMA_SUPPORT == 1)

static resource_mask_t dma_resource_mask(HW_DMA_CHANNEL num)
{
        const resource_mask_t res_mask[] = {
                RES_MASK(RES_ID_DMA_CH0), RES_MASK(RES_ID_DMA_CH1),
                RES_MASK(RES_ID_DMA_CH2), RES_MASK(RES_ID_DMA_CH3),
                RES_MASK(RES_ID_DMA_CH4), RES_MASK(RES_ID_DMA_CH5),
                RES_MASK(RES_ID_DMA_CH6), RES_MASK(RES_ID_DMA_CH7)
        };

        return res_mask[num];
}
# endif /* dg_configUSB_DMA_SUPPORT */
#endif /* OS_FREERTOS */

static void sys_usb_task(void* pvParameters)
{
        uint32_t        notif;
        OS_BASE_TYPE    ret;

        /* Initialize VBUS IRQ mask assuming that VBUS is not present
         * to start at a known state.
         */
        /* IRQ on rising edge triggered when USB is plugged in */
        hw_usb_program_vbus_irq_on_rising();
        /* IRQ on falling edge triggered when USB is plugged out */
        hw_usb_program_vbus_irq_on_falling();

        /* Check if VBUS is actually present in case USB was attached
         * before this task started. This check is needed as IRQs for plug-in/-out
         * are edge triggered, not level, and maybe the related event not be caught.
         */
        if (hw_usb_is_powered_by_vbus()) {
                /* VBUS IRQ has been missed (since it was not enabled when USB was attached).
                 * A fake interrupt is triggered to let the VBUS IRQ handler place the system in
                 * the correct state.
                 */
                NVIC_SetPendingIRQ(VBUS_IRQn);
        }

        while (true) {
                ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif, OS_TASK_NOTIFY_FOREVER);
                if (ret == OS_TASK_NOTIFY_FAIL) {
                        continue;
                }
                /* Handle VBUS event notification for plug-in/-out*/
                if ((notif & SYS_USB_TASK_MSG_VBUS_RISE) ||
                    (notif & SYS_USB_TASK_MSG_VBUS_FALL)) {
                        sys_usb_process_vbus_event();
#if (dg_configUSE_USB_ENUMERATION == 1)
                        sys_usb_remote_wakeup_ready = false;
#endif /* dg_configUSE_USB_ENUMERATION */
                }

#if (dg_configUSE_USB_ENUMERATION == 1)
                /* Handle USB_SUSPEND event */
                if (notif & SYS_USB_TASK_MSG_USB_SUSPEND) {
                         /* Host requested the device to suspend.
                          * We need to reduce power consumption.
                          * We will go from active to idle (WFI) */
                         if ( (sys_usb_is_suspended == false) && (sys_usb_is_pll96_activated == true) ) {
                                 sys_usb_remote_wakeup_ready = false;
                                 hw_usb_sd3_event(); // Take care of HW
                                 hw_usb_bus_event(UBE_SUSPEND); // Notify emUSB
                                 sys_usb_int_charger_hook_suspend_event(); // Notify Charger
                                 sys_usb_idle_on_suspend(true); // Go to IDLE (WFI)
                         }
                }

                /* Handle USB CLOCK RESET event to restore system's clock */
                if (notif & SYS_USB_TASK_MSG_SYS_CLOCK_RESET) {
                        if (sys_usb_is_pll96_activated) {
                                cm_sys_clk_set(USB_DEFAULT_CLK);
                                sys_usb_is_pll96_activated = false;
                        }
                }

                /* clock set to PLL96 MUST complete before handling usb events */
                if (notif & SYS_USB_TASK_MSG_SYS_CLOCK_PLL) {
                        if ( ! sys_usb_is_pll96_activated ) {
                                cm_sys_clk_set(sysclk_PLL96);
                                sys_usb_is_pll96_activated = true;
                        }
                }

                /* Handle USB RESET event */
                if (notif & SYS_USB_TASK_MSG_USB_RESET) {
                        OS_ASSERT(sys_usb_is_pll96_activated == true);
                        sys_usb_remote_wakeup_ready = false;
                        /* Host has exited suspend state. Go from idle to active */
                        sys_usb_idle_on_suspend(false);
                        /* Take care of HW */
                        hw_usb_reset_event();
                        /* Notify emUSB */
                        hw_usb_bus_event(UBE_RESET);
                }

                /* Handle USB REMOTE WAKEUP event */
                if (notif & SYS_USB_TASK_MSG_SYS_REMOTE_WAKEUP) {
                        /* Check if 5 ms after suspend has passed */
                        if (sys_usb_remote_wakeup_ready) {
                                /* Trigger in USB HW Block the Remote Wake-up request towards USB-Host */
                                hw_usb_request_remote_wakeup();
                                /* Wait minimum 10ms (HW requirement), suggested 15ms */
                                OS_DELAY_MS(15);
                                /* Set the USB-HW block only (not the USB Stack) to SUSPENDED mode
                                 * and wait for resume-event from Host */
                                hw_usb_set_hw_mode_to_suspended();
                        }
                }

                /* Handle USB RESUME event */
                if (notif & SYS_USB_TASK_MSG_USB_RESUME) {
                        /* Host exited suspend state.
                         * Restore operation and clocks.
                         * Go from idle to active
                         */
                        OS_ASSERT(sys_usb_is_pll96_activated == true);
                        sys_usb_remote_wakeup_ready = false;
                        /* Take care of HW */
                        hw_usb_resume_event();
                        /* Notify emUSB */
                        hw_usb_bus_event(UBE_RESUME);
                        /* Host has exited suspend state, go from idle to active */
                        sys_usb_idle_on_suspend(false);
                        /* Notify Charger */
                        sys_usb_int_charger_hook_resume_event();
                }
#endif /* (dg_configUSE_USB_ENUMERATION == 1) */
        }
}

static void sys_usb_process_vbus_event(void)
{
        /* Safe reading of register values */
        OS_DELAY_MS(SYS_USB_20ms_SAFE_READOUT_MARGIN);

        while (true) {
                /* Attest that the VBUS is available, in case that the used cable doesn't
                 * confront with the USB standards. These cable characteristics create bouncing leading to
                 * repeatedly plug-ins and plug-outs in ms */
                if (CRG_TOP->ANA_STATUS_REG & REG_MSK(CRG_TOP, ANA_STATUS_REG, VBUS_AVAILABLE)) {
                        // VBUS_AVAILABLE == 1
                        if (hw_usb_is_powered_by_vbus()) {
                                sys_usb_process_attach();
                                break;
                        }
                } else {
                        // VBUS_AVAILABLE == 0
                        sys_usb_process_detach();
                        break;
                }
                /* inconclusive, retry. Due to bouncing VBUS seems to be unavailable.
                 * The value is experimentally estimated
                 */
                OS_DELAY_MS(10);
        }
}

static void sys_usb_process_attach(void)
{
        if (sys_usb_is_process_attach_completed) {
                return;
        }

        /* Safe guard to properly call hw_usb_is_powered_by_vbus() */

        /* sys_usb_process_attach() and sys_usb_process_detach() work in a pipeline
         * fashion. That is the two processes will never interfere with each other.  This
         * is guaranteed by sys_usb_vbus_task(). As a result, there will be no race
         * threat for using sys_usb_is_process_attach_completed
         */
        sys_usb_is_process_attach_completed = true;

        /* System is not allowed to go to sleep when the USB is in use,
         * as the USB functionality will be lost. The USB host supplies the power to device,
         * so we don't care about power consumption and we should not drain the battery */
        pm_sleep_mode_request(pm_mode_active);

        /* When VBUS is used the hardware FSM closes DCDC. As DCDC
         * is powered exclusively by the VBAT1 which is shorted with VBAT2(system's supplies), if
         * we use DCDC and USB then:
         * 1. It will drain the battery.
         * 2. If we use Charger functionality, then VBAT must not have any loads.
         * 3. If the Battery is dead, it might fail to charge.
         * 4. If the device(e.g a USB dongle) works only on VBUS, VBAT is not used at all*/
        ad_pmu_dcdc_suspend();

        /* Propagate VBUS event to application level */
        sys_usb_ext_hook_attach();

#if (dg_configUSE_USB_ENUMERATION == 1) || (dg_configUSE_SYS_CHARGER == 1)
        /* Enable USB and charger interrupts and register USB ISR callback */
        hw_usb_enable_usb_interrupt(sys_usb_usb_isr_cb);
#endif /* (dg_configUSE_USB_ENUMERATION == 1) || (dg_configUSE_SYS_CHARGER == 1) */
        /* At this time Host sees a device that gets power from VBUS. Until enumeration Host
         * provide power to the device up to 100mA. Host is able to reduce this value if
         * enumeration didn't take place within a reasonable time
         */

#if (dg_configUSE_USB_ENUMERATION == 1)
        /* Check if the USB pads are properly configured. If USB pads
         * are configured with HW_GPIO_FUNC_GPIO and USB is used then
         * USB block might be permanently damaged */
        sys_usb_assert_usb_data_pin_conf();

 # if (dg_configUSB_DMA_SUPPORT == 1)
        if (usb_cfg.usb.use_dma) {
#  ifdef OS_FREERTOS
                resource_mask_t res_to_acquire = dma_resource_mask(usb_cfg.usb.tx_dma_channel) | dma_resource_mask(usb_cfg.usb.rx_dma_channel);

                /* Try to get the DMA channels needed for the USB RX/TX.
                 * If one of them is not available, then invalidate the DMA channels
                 * and set the flags for acquired dma channels to false
                 * so the USB will continue in interrupt mode for this plug-in
                 * and in Detach there will be no attempt to release
                 * any non-acquired resource which will trigger an assertion.
                 */
                if ( resource_acquire(res_to_acquire, 0) ) {
                        usb_cfg.usb.acquired_tx_dma = true;
                        usb_cfg.usb.acquired_rx_dma = true;
                } else {
                        usb_cfg.usb.acquired_tx_dma = false;
                        usb_cfg.usb.tx_dma_channel = HW_DMA_CHANNEL_INVALID;
                        usb_cfg.usb.acquired_rx_dma = false;
                        usb_cfg.usb.rx_dma_channel = HW_DMA_CHANNEL_INVALID;
                }
#  endif /* OS_FREERTOS */
                hw_usb_cfg(&usb_cfg.usb);
        }
# endif /* dg_configUSB_DMA_SUPPORT */
#endif /* (dg_configUSE_USB_ENUMERATION == 1) */

# if (dg_configUSE_SYS_CHARGER == 1)
        /* Power-on the USB pads without any pull-ups to use them for the charger detection */
        hw_usb_enable_usb_pads_without_pullup();
        /* Enable charger interrupts and start the Charger Detection */
        hw_usb_program_usb_irq();
        sys_usb_int_charger_hook_attach();
# else
        sys_usb_finalize_attach();
# endif /* dg_configUSE_SYS_CHARGER */
}

/*
 * The Detach process happens when the USB is unplugged
 */
static void sys_usb_process_detach(void)
{
        /* Do not proceed in detaching process in case of an uncompleted attach cycle */
        if (sys_usb_is_process_attach_completed) {
                sys_usb_is_process_attach_completed = false;

                /* First disable the interrupts */
                hw_usb_disable_usb_interrupt();
                /* Then disable the pads so there will be no fake events interrupts */
                hw_usb_disable_usb_pads();
                /* Resume to the previous state of DCDC */
                ad_pmu_dcdc_resume();

#if (dg_configUSE_USB_ENUMERATION == 1)
#if (dg_configUSB_SUSPEND_MODE == USB_SUSPEND_MODE_PAUSE)
                if (sys_usb_is_suspended == false) {
                        OS_ENTER_CRITICAL_SECTION();
                        hw_usb_enable_irqs_on_resume();
                        OS_LEAVE_CRITICAL_SECTION();
                }
#endif
                sys_usb_idle_on_suspend(false);
#endif /* (dg_configUSE_USB_ENUMERATION == 1) */
                /* Release the pm_mode_active and allow the system to resume sleep/wake cycles */
                pm_sleep_mode_release(pm_mode_active);

#if (dg_configUSE_SYS_CHARGER == 1)
                /* Inform system that USB is unplugged and handled as needed by Charger */
                sys_usb_int_charger_hook_detach();
#endif

#if (dg_configUSE_USB_ENUMERATION == 1)
                sys_usb_ext_hook_detach();
                hw_usb_bus_detach();
                /* Indicate the sys USB to lower the clock as we do not need the PLL96 */
                REG_SETF(CRG_TOP, CLK_CTRL_REG, USB_CLK_SRC, 0);
                USB->USB_MCTRL_REG = 0;
                OS_TASK_NOTIFY(sys_usb_task_h, SYS_USB_TASK_MSG_SYS_CLOCK_RESET, OS_NOTIFY_SET_BITS);
#if (dg_configUSB_DMA_SUPPORT == 1)
# ifdef OS_FREERTOS
                if (usb_cfg.usb.use_dma) {
                        /* If there are acquired DMA channels, then release them to be used
                         * by any other part of the code
                         */
                        if (usb_cfg.usb.acquired_rx_dma) {
                                resource_release(dma_resource_mask(usb_cfg.usb.rx_dma_channel));
                        }

                        if (usb_cfg.usb.acquired_tx_dma) {
                                resource_release(dma_resource_mask(usb_cfg.usb.tx_dma_channel));
                        }

                        /* Restore the initial user configuration to use for the next plug-in */
                        OPT_MEMCPY((void*)&usb_cfg, (void*)&usb_cfg_bkup, sizeof(usb_cfg_bkup));

                        /* Make sure that the acquired DMA resources flags are set to false */
                        usb_cfg.usb.acquired_rx_dma = false;
                        usb_cfg.usb.acquired_tx_dma = false;
                }
# endif /* OS_FREERTOS */
#endif /* dg_configUSB_DMA_SUPPORT */
#endif /* (dg_configUSE_USB_ENUMERATION == 1) */
        }
}

#if (dg_configUSE_USB_ENUMERATION == 1)
#if (dg_configUSB_DMA_SUPPORT == 1)
void sys_usb_cfg(const sys_usb_driver_conf_t *cfg)
{
        usb_cfg.usb.use_dma = cfg->usb.use_dma;
        if (usb_cfg.usb.use_dma) {
                usb_cfg.usb.rx_dma_channel = cfg->usb.rx_dma_channel;
                usb_cfg.usb.tx_dma_channel = cfg->usb.tx_dma_channel;
                usb_cfg.usb.rx_dma_prio = cfg->usb.rx_dma_prio;
                usb_cfg.usb.tx_dma_prio = cfg->usb.tx_dma_prio;
        }

        OPT_MEMCPY((void*)&usb_cfg_bkup, (void*)&usb_cfg, sizeof(usb_cfg));
}
#endif /* dg_configUSB_DMA_SUPPORT */
#endif /* dg_configUSE_USB_ENUMERATION */

void sys_usb_init(void)
{
        sys_usb_is_process_attach_completed = false;

#if (dg_configUSE_USB_ENUMERATION == 1)
        /* Set the SDK USB callbacks */
        set_sdk_callbacks_1469x();
        /* Set the emUSB (Segger USB Stack) */
        set_emusb_1469x_driver();
#endif /* (dg_configUSE_USB_ENUMERATION == 1) */
        OS_TASK_CREATE("VBUS",                           /* The text name assigned to the task, for
                                                            debug only; not used by the kernel. */
                        sys_usb_task,                    /* The function that implements the task. */
                        NULL,                            /* The parameter passed to the task. */
                        configMINIMAL_STACK_SIZE * OS_STACK_WORD_SIZE,
                                                         /* The number of bytes to allocate to the
                                                            stack of the task. */
                        OS_TASK_PRIORITY_HIGHEST - 2,    /* The priority assigned to the task. */
                        sys_usb_task_h);                 /* The task handle */

        OS_ASSERT(sys_usb_task_h);

        /* Enable the VBUS Interrupt (PHY) in NVIC */
        hw_usb_enable_vbus_interrupt(sys_usb_vbus_isr_cb);
}

#if (dg_configUSE_USB_ENUMERATION == 1)
void sys_usb_finalize_attach(void)
{
        /* USB Pads will be powered-on when the USB will be in use. */
        hw_usb_disable_usb_pads();
        /* select initialize clock for the USB */
        hw_usb_init();
        /* enable the USB block */
        hw_usb_node_enable();
        /* prepare the USB block */
        hw_usb_bus_attach();

        /* Now that everything is ready, announce device presence to the USB host */
        hw_usb_node_attach();

        /* Call USB functionality at application level
         * where the enumeration is implemented */
        sys_usb_ext_hook_begin_enumeration();
}

bool sys_usb_remote_wakeup(void)
{
        bool ret = false;

        if (sys_usb_is_suspended) {
                if (sys_usb_remote_wakeup_ready) {
                        if ((OS_TASK_NOTIFY(sys_usb_task_h, SYS_USB_TASK_MSG_SYS_REMOTE_WAKEUP, OS_NOTIFY_SET_BITS) == OS_TASK_NOTIFY_SUCCESS)) {
                                /* The application requested for remote wake-up
                                 * must stay blocked for at least 20 ms.
                                 * Safer to be a little longer */
                                OS_DELAY_MS(30);
                                ret = true;
                        } else {
                                ret = false;
                        }
                }
        }

        return ret;
}
#endif /* dg_configUSE_USB_ENUMERATION */

/**
 * \brief USB Interrupt handling function.
 *
 */
void hw_usb_interrupt_handler(uint32_t status)
{
        __UNUSED uint16_t maev = status;

        /* USB events have higher priority than charger event */
#if (dg_configUSE_USB_ENUMERATION == 1)

        /* Handshake packets for EP0 used in enumeration */
        if (maev & USB_USB_MAEV_REG_USB_EP0_NAK_Msk) {
                hw_usb_nak_event_ep0();
        }

        /* EP0 Data Events */
        if (maev & USB_USB_MAEV_REG_USB_EP0_TX_Msk) {
                hw_usb_tx_ep(0);
        }

        if (maev & USB_USB_MAEV_REG_USB_EP0_RX_Msk) {
                hw_usb_rx_ep0();
        }

        /* Handshake packets for EP1-EP6 used in enumeration */
        if (maev & USB_USB_MAEV_REG_USB_FRAME_Msk) {
                hw_usb_frame_event();
        }

        if (maev & USB_USB_MAEV_REG_USB_NAK_Msk) {
                hw_usb_nak_event();
        }

        /* EP1-EP6 Data Events */
        if (maev & USB_USB_MAEV_REG_USB_TX_EV_Msk) {
                hw_usb_tx_event();
        }

        if (maev & USB_USB_MAEV_REG_USB_RX_EV_Msk) {
                hw_usb_rx_event();
        }

        /* VBUS events */
        if (maev & USB_USB_MAEV_REG_USB_ALT_Msk) {
                uint8_t altev;

                altev = USB->USB_ALTEV_REG;
                altev &= USB->USB_ALTMSK_REG;

                /* Transition to NodeReset.
                 * Reset takes place in enumeration (2 Resets are sent) for synch,
                 * in other cases Reset means that the USB device is unresponsive */
                if (altev & USB_USB_ALTEV_REG_USB_RESET_Msk) {
                        /* clear the event */
                        REG_CLR_BIT(USB, USB_ALTMSK_REG, USB_M_RESET);
                        /* Notify the sys_usb task for the rest of the actions needed for the event */
                        OS_TASK_NOTIFY_FROM_ISR(sys_usb_task_h,
                                SYS_USB_TASK_MSG_SYS_CLOCK_PLL | SYS_USB_TASK_MSG_USB_RESET,
                                OS_NOTIFY_SET_BITS);

                        /* RESET received, do not process anything else */
                        return;
                }

                /* Transition to NodeSuspend */
                if (altev & USB_USB_ALTEV_REG_USB_SD3_Msk) {
                        /* make sure the SD5 Interrupt is enabled */
                        REG_SET_BIT(USB, USB_ALTMSK_REG, USB_M_SD5);
                        /* Notify the sys_usb task for the rest of the actions needed for the event */
                        OS_TASK_NOTIFY_FROM_ISR(sys_usb_task_h,
                                SYS_USB_TASK_MSG_SYS_CLOCK_RESET | SYS_USB_TASK_MSG_USB_SUSPEND,
                                OS_NOTIFY_SET_BITS);
                }

                /* Ensure at least 5 ms of Idle on the USB for the case of Remote Wakeup */
                if (altev & USB_USB_ALTEV_REG_USB_SD5_Msk) {
                        /* block the SD5 interrupt in USB_ALTMSK_REG until next suspend*/
                        REG_CLR_BIT(USB, USB_ALTMSK_REG, USB_M_SD5);
                        sys_usb_remote_wakeup_ready = true;
                }

                /* Transition to NodeResume */
                if (altev & USB_USB_ALTEV_REG_USB_RESUME_Msk) {
                        /* Notify the sys_usb task for the rest of the actions needed for the event */
                        OS_TASK_NOTIFY_FROM_ISR(sys_usb_task_h,
                                SYS_USB_TASK_MSG_USB_RESUME | SYS_USB_TASK_MSG_SYS_CLOCK_PLL,
                                OS_NOTIFY_SET_BITS);
                }
        }

#endif /* (dg_configUSE_USB_ENUMERATION == 1) */

#if (dg_configUSE_SYS_CHARGER == 1)
        /* Charger Events */
        if (maev & REG_MSK(USB, USB_MAEV_REG, USB_CH_EV)) {
                sys_usb_int_charger_hook_ch_event();
        }
#endif
}

/************************************** Helper Functions ******************************************/

#if (dg_configUSE_USB_ENUMERATION == 1)
static void sys_usb_assert_usb_data_pin_conf(void)
{
        HW_GPIO_MODE mode;
        HW_GPIO_FUNC func;

        hw_gpio_get_pin_function(HW_GPIO_PORT_0, HW_GPIO_PIN_14, &mode, &func);

        OS_ASSERT(mode == HW_GPIO_MODE_INPUT);
        OS_ASSERT(func == HW_GPIO_FUNC_USB);

        hw_gpio_get_pin_function(HW_GPIO_PORT_0, HW_GPIO_PIN_15, &mode, &func);

        OS_ASSERT(mode == HW_GPIO_MODE_INPUT);
        OS_ASSERT(func == HW_GPIO_FUNC_USB);
}

extern bool is_suspended;
static void sys_usb_idle_on_suspend(bool set_idle)
{
        if (set_idle) {
#if (dg_configUSB_SUSPEND_MODE != USB_SUSPEND_MODE_NONE)
                if (!sys_usb_is_suspended) {
                        pm_sleep_mode_request(pm_mode_idle);
                        pm_sleep_mode_release(pm_mode_active);
                }
#endif
                sys_usb_is_suspended = true;
        } else {
#if (dg_configUSB_SUSPEND_MODE != USB_SUSPEND_MODE_NONE)
                if (sys_usb_is_suspended) {
                        pm_sleep_mode_request(pm_mode_active);
                        pm_sleep_mode_release(pm_mode_idle);
                }
#endif
                sys_usb_is_suspended = false;
        }

        is_suspended = sys_usb_is_suspended;
}
#endif /* (dg_configUSE_USB_ENUMERATION == 1) */

#endif /* dg_configUSE_SYS_USB */
/**
 \}
 \}
 \}
 */
