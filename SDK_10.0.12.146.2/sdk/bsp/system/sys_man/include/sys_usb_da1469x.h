/**
 * \addtogroup MID_SYS_SERVICES
 * \{
 * \addtogroup USB_SERVICE USB System Service
 *
 * \brief System USB
 *
 * \{
 */

/**
 ****************************************************************************************
 *
 * @file sys_usb_da1469x.h
 *
 * @brief System USB header file.
 *
 * Copyright (C) 2018-2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */


#ifndef SYS_USB_DA1469x_H_
#define SYS_USB_DA1469x_H_

#include <sdk_defs.h>
#include <hw_usb.h>

#if (dg_configUSE_USB_ENUMERATION == 1)
#if (dg_configUSB_DMA_SUPPORT == 1)
/**
 * \brief USB driver configuration
 *
 * Configuration of  USB low level driver(s)
 *
 * \note There may be more than one driver configurations needed (e.g DMA)
 *
 */
typedef struct {
        usb_config usb;         /**< Low level driver configuration */
}sys_usb_driver_conf_t;

/**
 * \brief Initialize sys_usb service.
 *
 * \param[in] cfg the configuration struct of usb
 */
void sys_usb_cfg(const sys_usb_driver_conf_t *cfg);

#endif /* dg_configUSB_DMA_SUPPORT */

/**
 * \brief Finalize the attach procedure.
 *        Called from sys_usb or sys_charger, depending on configuration
 *
 */
void sys_usb_finalize_attach(void);
#endif /* dg_configUSE_USB_ENUMERATION */

/**
 * \brief Initialize USB and VBUS events handling subsystem.
 * The Function creates the sys_usb task which is the recipient
 * of the USB events and VBUS events.
 * The VBUS events are the starting point for both Charger and USB-Data
 * functionality.
 * When the USB-Data functionality is enabled the sys_usb_init()
 * initialize also the call-backs between the emUSB and the Lower Level
 * USB/VBUS handling (LLD, sys_usb).
 * If only Charger functionality is enabled then only the VBUS events are
 * handled by the Lower level USB/VBUS level
 *
 * The USB/Charger interrupt is enabled as last action in the sys_usb_init
 */
void sys_usb_init(void);

/**
* \brief Device-initiated wake up of the USB node.
 *        Resumes to normal operation of the device from suspended state,
 *        serving an event trigger detection in the application layer.
 *
 * \return \li true in success
 *         \li false in fail
 */
bool sys_usb_remote_wakeup(void);

/******************** Weak functions to be implemented, if needed, by the application code ********/

/**
 * \brief Notification hook to be called when VBUS is attached.
 *
 */
__WEAK void sys_usb_ext_hook_attach(void);

/**
 * \brief Notification hook to be called when VBUS is detached.
 *
 */
__WEAK void sys_usb_ext_hook_detach(void);

/**
 * \brief Notification hook to be called when charger detection,
 * if any, is completed, and the device can begin enumeration.
 *
 */
__WEAK void sys_usb_ext_hook_begin_enumeration(void);

#endif /* SYS_USB_DA1469x_H_ */


/**
\}
\}
*/
