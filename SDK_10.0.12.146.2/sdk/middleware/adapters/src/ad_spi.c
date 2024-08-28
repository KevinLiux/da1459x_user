/**
 * \addtogroup MID_SYS_ADAPTERS
 * \{
 * \addtogroup SPI_ADAPTER SPI Adapter
 *
 * \brief Adapter for SPI controller
 *
 * \{
 */

/**
 ****************************************************************************************
 *
 * @file ad_spi.c
 *
 * @brief SPI Adapter implementation
 *
 * Copyright (C) 2015-2021 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#if dg_configSPI_ADAPTER

#include <stdint.h>
#include "sdk_defs.h"
#include "interrupts.h"
#include "sys_power_mgr.h"
#include "hw_sys.h"
#include "hw_gpio.h"
#include "sdk_list.h"
#include "sys_bsr.h"


#include <stdarg.h>
#include "ad_spi.h"
#include "hw_spi.h"
#include "platform_devices.h"
#include "resmgmt.h"


typedef struct {
        /**< SPI controller current configuration */
        const ad_spi_controller_conf_t *conf;
        /**< Internal data */
        OS_TASK  owner; /**< The task which opened the controller */
        OS_EVENT event; /**< Semaphore for async calls  */
        OS_MUTEX busy;  /**< Semaphore for thread safety */
} ad_spi_data_t;

__RETAINED static ad_spi_data_t spi1_data;
__RETAINED static ad_spi_data_t spi2_data;

#define AD_SPI_IO_SIZE          (3)
#define AD_SPI_HANDLE_IS_VALID(__handle) (((__handle == &spi1_data) || (__handle == &spi2_data)) && (((ad_spi_data_t*) __handle)->conf != NULL))

void ad_spi_init(void)
{
        OS_EVENT_CREATE(spi1_data.event);
        OS_MUTEX_CREATE(spi1_data.busy);
        OS_EVENT_CREATE(spi2_data.event);
        OS_MUTEX_CREATE(spi2_data.busy);
}

#if (HW_SPI_DMA_SUPPORT == 1)
static void ad_spi_res_acquire(RES_ID id, HW_DMA_CHANNEL dma_channel)
#else
static void ad_spi_res_acquire(RES_ID id)
#endif /* HW_SPI_DMA_SUPPORT */
{
#if (HW_SPI_DMA_SUPPORT == 1)
        /* The resource is acquired via the low channel.
         * The high channel is checked for validity.
         */
        if (dma_channel + 1 < HW_DMA_CHANNEL_INVALID) {
                resource_acquire(RES_MASK(id) | RES_MASK(RES_ID_DMA_CH0 + dma_channel) |
                        RES_MASK(RES_ID_DMA_CH0 + dma_channel + 1), RES_WAIT_FOREVER);
        }
        else
#endif /* HW_SPI_DMA_SUPPORT */
        {
                resource_acquire(RES_MASK(id), RES_WAIT_FOREVER);
        }
}

#if (HW_SPI_DMA_SUPPORT == 1)
static void ad_spi_res_release(RES_ID id, HW_DMA_CHANNEL dma_channel)
#else
static void ad_spi_res_release(RES_ID id)
#endif /* HW_SPI_DMA_SUPPORT */
{
#if (HW_SPI_DMA_SUPPORT == 1)
        /* The resource is acquired via the low channel.
         * The high channel is checked for validity.
         */
        if (dma_channel + 1 < HW_DMA_CHANNEL_INVALID) {
                resource_release(RES_MASK(id) | RES_MASK(RES_ID_DMA_CH0 + dma_channel) |
                        RES_MASK(RES_ID_DMA_CH0 + dma_channel + 1));
        }
        else
#endif /* HW_SPI_DMA_SUPPORT */
        {
                resource_release(RES_MASK(id));
        }
}

static bool config_io(const ad_spi_io_conf_t *io, AD_IO_CONF_STATE state)
{
        uint8_t ret = AD_IO_ERROR_NONE;

        if (!AD_IO_PIN_PORT_VALID(io->spi_do.port, io->spi_do.pin)) {
                /* assume only CLK and DI are needed */
                ret |= ad_io_configure(&io->spi_clk, 2, io->voltage_level, state);
        } else if (!AD_IO_PIN_PORT_VALID(io->spi_di.port, io->spi_di.pin)) {
                /* assume only CLK and DO are needed */
                ret |= ad_io_configure(&io->spi_do, 2, io->voltage_level, state);
        } else {
                ret |= ad_io_configure(&io->spi_do, AD_SPI_IO_SIZE, io->voltage_level, state);
        }
        ret |= ad_io_configure(io->spi_cs, io->cs_cnt, io->voltage_level, state);

        return (AD_IO_ERROR_NONE == ret);
}

static void set_pad_latches(const ad_spi_io_conf_t *io, AD_IO_CONF_STATE state)
{
        if (!AD_IO_PIN_PORT_VALID(io->spi_do.port, io->spi_do.pin)) {
                /* assume only CLK and DI are needed */
                ad_io_set_pad_latch(&io->spi_clk, 2, state);
        } else if (!AD_IO_PIN_PORT_VALID(io->spi_di.port, io->spi_di.pin)) {
                /* assume only CLK and DO are needed */
                ad_io_set_pad_latch(&io->spi_do, 2, state);
        } else {
                ad_io_set_pad_latch(&io->spi_do, AD_SPI_IO_SIZE, state);
        }
        ad_io_set_pad_latch(io->spi_cs, io->cs_cnt, state);
}

ad_spi_handle_t ad_spi_open(const ad_spi_controller_conf_t *conf)
{
        OS_ASSERT(conf);
        OS_ASSERT(conf->drv);
        OS_ASSERT(conf->io);
        ad_spi_data_t *spi = ((conf->id == HW_SPI1) ? &spi1_data : &spi2_data);
        OS_ASSERT(spi);

        pm_sleep_mode_request(pm_mode_idle);

        RES_ID res_id = ((conf->id == HW_SPI1) ? RES_ID_SPI1 : RES_ID_SPI2);
#if (HW_SPI_DMA_SUPPORT == 1)
        uint8_t dma_channel = conf->drv->spi.rx_dma_channel;
        OS_ASSERT(dma_channel + 1 < HW_DMA_CHANNEL_INVALID);
#endif /* HW_SPI_DMA_SUPPORT */

        sys_sw_bsr_acquire(SW_BSR_MASTER_SYSCPU,
                (conf->id == HW_SPI1) ? BSR_PERIPH_ID_SPI1 : BSR_PERIPH_ID_SPI2);
#if (HW_SPI_DMA_SUPPORT == 1)
        ad_spi_res_acquire(res_id, dma_channel);
#else
        ad_spi_res_acquire(res_id);
#endif /* HW_SPI_DMA_SUPPORT */

        hw_sys_pd_com_enable();

        if (!config_io(conf->io, AD_IO_CONF_ON)) {
#if (HW_SPI_DMA_SUPPORT == 1)
                ad_spi_res_release(res_id, dma_channel);
#else
                ad_spi_res_release(res_id);
#endif /* HW_SPI_DMA_SUPPORT */

                /* Configure the GPIOs for the desired OFF state.
                 * Some of them might have been configured in the process of
                 * applying the IO configuration settings.
                 * Checking the return for error is not needed,
                 * since we cannot return anything different
                 * than NULL to indicate a problem */
                config_io(conf->io, AD_IO_CONF_OFF);
                hw_sys_pd_com_disable();
                sys_sw_bsr_release(SW_BSR_MASTER_SYSCPU,
                (conf->id == HW_SPI1) ? BSR_PERIPH_ID_SPI1 : BSR_PERIPH_ID_SPI2);
                pm_sleep_mode_release(pm_mode_idle);
                return NULL;
        }

        spi->owner = OS_GET_CURRENT_TASK();

        OS_ENTER_CRITICAL_SECTION();
        hw_spi_enable(conf->id, 1);
        OS_LEAVE_CRITICAL_SECTION();

        spi->conf = conf;

        if (ad_spi_reconfig(spi, conf->drv) != AD_SPI_ERROR_NONE) {
                ASSERT_WARNING(0);
                /* If the controller is not in use, then make sure
                 * is turned OFF */
                hw_spi_enable(conf->id, 0);

                /* Configure the GPIOs for the desired OFF state.
                 * Checking the return for error is not needed,
                 * since we cannot return anything different
                 * than NULL to indicate a problem */
                config_io(conf->io, AD_IO_CONF_OFF);

                spi->conf = NULL;
#if (HW_SPI_DMA_SUPPORT == 1)
                ad_spi_res_release(res_id, dma_channel);
#else
                ad_spi_res_release(res_id);
#endif /* HW_SPI_DMA_SUPPORT */
                hw_sys_pd_com_disable();
                sys_sw_bsr_release(SW_BSR_MASTER_SYSCPU,
                (conf->id == HW_SPI1) ? BSR_PERIPH_ID_SPI1 : BSR_PERIPH_ID_SPI2);
                pm_sleep_mode_release(pm_mode_idle);
                return NULL;
        }
        /* Everything is configured and ready.
         * Now it is safe to enable the Latches. */
        set_pad_latches(conf->io, AD_IO_PAD_LATCHES_OP_ENABLE);
        return spi;
}

int ad_spi_reconfig(ad_spi_handle_t handle, const ad_spi_driver_conf_t *drv_conf)
{
        ad_spi_data_t *spi = (ad_spi_data_t *) handle;

        OS_ASSERT(drv_conf);
        OS_ASSERT(spi->conf->drv);

        if (!AD_SPI_HANDLE_IS_VALID(handle)) {
                OS_ASSERT(0);
                return AD_SPI_ERROR_HANDLE_INVALID;
        }

        OS_MUTEX_GET(spi->busy, OS_MUTEX_FOREVER);

        if (hw_spi_is_occupied(spi->conf->id)) {
                OS_MUTEX_PUT(spi->busy);
                return AD_SPI_ERROR_TRANSF_IN_PROGRESS;
        }

#if (HW_SPI_DMA_SUPPORT == 1)
        if (spi->conf->drv->spi.rx_dma_channel != drv_conf->spi.rx_dma_channel) {
                ASSERT_WARNING(0);
                OS_MUTEX_PUT(spi->busy);
                return AD_SPI_ERROR_CONFIG_DMA_CHANNEL_INVALID;
        }

        if (spi->conf->drv->spi.tx_dma_channel != drv_conf->spi.tx_dma_channel) {
                ASSERT_WARNING(0);
                OS_MUTEX_PUT(spi->busy);
                return AD_SPI_ERROR_CONFIG_DMA_CHANNEL_INVALID;
        }
#endif /* HW_SPI_DMA_SUPPORT */

        if (spi->conf->drv->spi.smn_role != drv_conf->spi.smn_role) {
                ASSERT_WARNING(0);
                OS_MUTEX_PUT(spi->busy);
                return AD_SPI_ERROR_CONFIG_SPI_ROLE_INVALID;
        }

          /* check if the CS pin is the configured one */
        if (spi->conf->drv->spi.smn_role == HW_SPI_MODE_MASTER) {
                uint8_t cs_cnt = spi->conf->io->cs_cnt;
                const ad_io_conf_t* cs_ports = spi->conf->io->spi_cs;
                uint8 cs_configured = 0;
                uint8_t i = 0;

                for (i = 0; i< cs_cnt; i++) {
                        cs_configured = ((cs_ports->port == drv_conf->spi.cs_pad.port) &&
                                (cs_ports->pin == drv_conf->spi.cs_pad.pin));
                        if (cs_configured ) {
                                break;
                        }
                        cs_ports++;
                }

                if (!cs_configured) {
                        ASSERT_WARNING(0);
                        OS_MUTEX_PUT(spi->busy);
                        return AD_SPI_ERROR_CONFIG_SPI_CS_INVALID;
                }
        }

        hw_spi_init(spi->conf->id, &drv_conf->spi);

        OS_MUTEX_PUT(spi->busy);

        return AD_SPI_ERROR_NONE;
}

int ad_spi_close(ad_spi_handle_t handle, bool force)
{
        ad_spi_data_t *spi = (ad_spi_data_t *) handle;

        if (!AD_SPI_HANDLE_IS_VALID(handle)) {
                OS_ASSERT(0);
                return AD_SPI_ERROR_HANDLE_INVALID;
        }

        HW_SPI_ID id = spi->conf->id;
        RES_ID res_id = ((id == HW_SPI1) ? RES_ID_SPI1 : RES_ID_SPI2);
#if (HW_SPI_DMA_SUPPORT == 1)
        int8_t dma_channel = spi->conf->drv->spi.rx_dma_channel;
#endif /* HW_SPI_DMA_SUPPORT */
        /* check for ongoing transactions */

        if (!force && hw_spi_is_occupied(id)) {
                return AD_SPI_ERROR_TRANSF_IN_PROGRESS;
        }

        hw_spi_deinit(id);

        if (!config_io(spi->conf->io, AD_IO_CONF_OFF)) {
                return AD_SPI_ERROR_IO_CFG_INVALID;
        }
        set_pad_latches(spi->conf->io, AD_IO_PAD_LATCHES_OP_DISABLE);
        hw_sys_pd_com_disable();
        spi->owner = NULL;
        spi->conf = NULL;

#if (HW_SPI_DMA_SUPPORT == 1)
        ad_spi_res_release(res_id, dma_channel);
#else
        ad_spi_res_release(res_id);
#endif /* HW_SPI_DMA_SUPPORT */

        sys_sw_bsr_release(SW_BSR_MASTER_SYSCPU,
                (id == HW_SPI1) ? BSR_PERIPH_ID_SPI1 : BSR_PERIPH_ID_SPI2);

        pm_sleep_mode_release(pm_mode_idle);

        return AD_SPI_ERROR_NONE;
}

HW_SPI_ID ad_spi_get_hw_spi_id(ad_spi_handle_t handle)
{
        ad_spi_data_t *spi = (ad_spi_data_t *) handle;
        return spi->conf->id;
}

static void ad_spi_wait_event( void *p, uint16_t transferred)
{

        ad_spi_data_t *spi = (ad_spi_data_t *) p;
        OS_EVENT_SIGNAL_FROM_ISR(spi->event);
}

void ad_spi_activate_cs(ad_spi_handle_t handle)
{
        ad_spi_data_t *spi = (ad_spi_data_t *) handle;

        const HW_SPI_ID id = spi->conf->id;
        /* The task must own the controller */
        OS_ASSERT(id);

        if (!hw_spi_is_slave(id)) {
                hw_spi_set_cs_low(id);
        }
}

void ad_spi_deactivate_cs(ad_spi_handle_t handle)
{
        ad_spi_data_t *spi = (ad_spi_data_t *) handle;

        const HW_SPI_ID id = spi->conf->id;
        /* The task must own the controller */
        OS_ASSERT(id);

        if (!hw_spi_is_slave(id)) {
                hw_spi_set_cs_high(id);
        }
}

void ad_spi_deactivate_cs_when_spi_done(ad_spi_handle_t handle)
{
        ad_spi_data_t *spi = (ad_spi_data_t *) handle;

        const HW_SPI_ID id = spi->conf->id;
        /* The task must own the controller */
        OS_ASSERT(id);
        hw_spi_wait_while_busy(id);

        ad_spi_deactivate_cs(handle);
}

int ad_spi_write(ad_spi_handle_t handle, const uint8_t *wbuf, size_t wlen)
{

        ad_spi_data_t *spi = (ad_spi_data_t *) handle;

        if (!AD_SPI_HANDLE_IS_VALID(handle)) {
                OS_ASSERT(0);
                return AD_SPI_ERROR_HANDLE_INVALID;
        }

        const HW_SPI_ID id = spi->conf->id;

        OS_MUTEX_GET(spi->busy, OS_MUTEX_FOREVER);

        if (hw_spi_is_occupied(id)) {
                OS_MUTEX_PUT(spi->busy);
                return AD_SPI_ERROR_TRANSF_IN_PROGRESS;
        }
        hw_spi_writeread_buf(id, wbuf, NULL, wlen, ad_spi_wait_event, spi);

        OS_EVENT_WAIT(spi->event, OS_EVENT_FOREVER);

        OS_MUTEX_PUT(spi->busy);
        return AD_SPI_ERROR_NONE;
}

int ad_spi_read(ad_spi_handle_t handle, uint8_t *rbuf, size_t rlen)
{
        ad_spi_data_t *spi = (ad_spi_data_t *) handle;

        if (!AD_SPI_HANDLE_IS_VALID(handle)) {
                OS_ASSERT(0);
                return AD_SPI_ERROR_HANDLE_INVALID;
        }

        const HW_SPI_ID id = spi->conf->id;

        OS_MUTEX_GET(spi->busy, OS_MUTEX_FOREVER);

        if (hw_spi_is_occupied(id)) {
                OS_MUTEX_PUT(spi->busy);
                return AD_SPI_ERROR_TRANSF_IN_PROGRESS;
        }

        hw_spi_read_buf(id, rbuf, rlen, ad_spi_wait_event, spi);

        OS_EVENT_WAIT(spi->event, OS_EVENT_FOREVER);

        OS_MUTEX_PUT(spi->busy);
        return AD_SPI_ERROR_NONE;
}

#if CONFIG_SPI_USE_ASYNC_TRANSACTIONS

int ad_spi_write_async(ad_spi_handle_t handle, const uint8_t *wbuf, size_t wlen,
                                                ad_spi_user_cb cb, void *user_data)
{
        ad_spi_data_t *spi = (ad_spi_data_t *) handle;

        if (!AD_SPI_HANDLE_IS_VALID(handle)) {
                OS_ASSERT(0);
                return AD_SPI_ERROR_HANDLE_INVALID;
        }

        const HW_SPI_ID id = spi->conf->id;

        OS_MUTEX_GET(spi->busy, OS_MUTEX_FOREVER);

        if (hw_spi_is_occupied(id)) {
                OS_MUTEX_PUT(spi->busy);
                return AD_SPI_ERROR_TRANSF_IN_PROGRESS;
        }

        hw_spi_write_buf(id, wbuf, wlen, cb, user_data);
        OS_MUTEX_PUT(spi->busy);
        return AD_SPI_ERROR_NONE;
}

int ad_spi_read_async(ad_spi_handle_t handle, const uint8_t *rbuf, size_t rlen,
                                                ad_spi_user_cb cb, void *user_data)
{
        ad_spi_data_t *spi = (ad_spi_data_t *) handle;

        if (!AD_SPI_HANDLE_IS_VALID(handle)) {
                OS_ASSERT(0);
                return AD_SPI_ERROR_HANDLE_INVALID;
        }

        const HW_SPI_ID id = spi->conf->id;

        OS_MUTEX_GET(spi->busy, OS_MUTEX_FOREVER);


        if (hw_spi_is_occupied(id)) {
                OS_MUTEX_PUT(spi->busy);
                return AD_SPI_ERROR_TRANSF_IN_PROGRESS;
        }

        hw_spi_read_buf(id, (uint8_t *)rbuf, (uint16_t)rlen, cb, user_data);
        OS_MUTEX_PUT(spi->busy);
        return AD_SPI_ERROR_NONE;
}

int ad_spi_write_read_async(ad_spi_handle_t handle, const uint8_t *wbuf, size_t wlen,
                         uint8_t *rbuf, size_t rlen, ad_spi_user_cb cb, void *user_data)
{
        ad_spi_data_t *spi = (ad_spi_data_t *) handle;

        if (!AD_SPI_HANDLE_IS_VALID(handle)) {
                OS_ASSERT(0);
                return AD_SPI_ERROR_HANDLE_INVALID;
        }

        const HW_SPI_ID id = spi->conf->id;

        OS_MUTEX_GET(spi->busy, OS_MUTEX_FOREVER);


        if (hw_spi_is_occupied(id)) {
                OS_MUTEX_PUT(spi->busy);
                return AD_SPI_ERROR_TRANSF_IN_PROGRESS;
        }

        hw_spi_writeread_buf(id, wbuf, rbuf, wlen, cb, user_data);
        OS_MUTEX_PUT(spi->busy);
        return AD_SPI_ERROR_NONE;
}
#endif /* CONFIG_SPI_USE_ASYNC_TRANSACTIONS */

int ad_spi_io_config(HW_SPI_ID id, const ad_spi_io_conf_t *io, AD_IO_CONF_STATE state)
{
        /* SPI clk should be at least configured*/
        if (!AD_IO_PIN_PORT_VALID(io->spi_clk.port, io->spi_clk.pin)) {
                return AD_SPI_ERROR_NO_SPI_CLK_PIN;
        }

        hw_sys_pd_com_enable();

        if (!config_io(io, state)) {
                return AD_SPI_ERROR_IO_CFG_INVALID;
        }

        set_pad_latches(io, AD_IO_PAD_LATCHES_OP_TOGGLE);
        hw_sys_pd_com_disable();
        return AD_SPI_ERROR_NONE;
}

ADAPTER_INIT(ad_spi_adapter, ad_spi_init);

#endif /* dg_configSPI_ADAPTER */

/**
 * \}
 * \}
 * \}
 */
