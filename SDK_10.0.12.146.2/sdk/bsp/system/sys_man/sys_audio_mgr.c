/**
 ****************************************************************************************
 *
 * @file sys_audio_mgr.c
 *
 * @brief System Audio manager
 *
 * Copyright (C) 2019-2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */
#if dg_configSYS_AUDIO_MGR

#include "sdk_defs.h"
#ifdef OS_FREERTOS
#include "osal.h"
#include "resmgmt.h"
#endif
#include "hw_apu_src.h"
#include "sys_power_mgr.h"
#include "sys_audio_mgr.h"

#define MAX_NO_PATHS                    3        /* the max number of concurrently supported audio data paths.
                                                  * When this macro is used as path index, its value indicates the invalid path index*/

#define APU_DEFAULT_SAMPLING_RATE       8000     /* Default sampling rate */
#define APU_DEFAULT_BITS_DEPTH          16       /* Default bit depth */
#define APU_DEFAULT_PDM_FREQUENCY       4000000  /* Default frequency for PDM */
#define APU_DEFAULT_SRC_CLK             32000000 /* Default frequency for SRC clock */

/* PCM defaults */
#define APU_DEFAULT_OUTPUT_MODE         (HW_PCM_DO_OUTPUT_PUSH_PULL)          /* Default PCM Output Mode */
#define APU_DEFAULT_PCM_CYCLE_PER_BIT   (HW_PCM_ONE_CYCLE_PER_BIT)            /* Default PCM cycle per bit */
#define APU_DEFAULT_FSC_DELAY           (HW_PCM_FSC_STARTS_SYNCH_TO_MSB_BIT)  /* Default PCM FSC delay */

#define BIT_DEPTH_MAX            32   /* the max number of bit depth */
#define CHANNEL_NUM_MAX          2    /* the max number of audio channels */

#ifdef OS_FREERTOS
#define AUDIO_MALLOC            OS_MALLOC
#define AUDIO_FREE              OS_FREE
#else
#define AUDIO_MALLOC            malloc
#define AUDIO_FREE              free
#endif

typedef enum {
        DIRECTION_INPUT,       /* Audio interface's data direction input */
        DIRECTION_OUTPUT,      /* Audio interface's data direction output */
} SYS_AUDIO_MGR_DIRECTION;

typedef enum {
        DMA_DIR_MEM_TO_PERIPH,  /* DMA direction from memory to peripheral */
        DMA_DIR_PERIPH_TO_MEM,  /* DMA direction from peripheral to memory */
} SYS_AUDIO_MGR_DMA_DIR;

typedef struct {
        HW_DMA_CHANNEL dma_channel_number;              /* DMA channel number */
        sys_audio_mgr_buffer_ready_cb cb;               /* DMA callback */
        sys_audio_path_t *path;                         /* Audio data path where DMA is used */
        sys_audio_mgr_buffer_data_block_t buff_block;   /* DMA buffer data block */
        uint8_t bit_depth;                              /* Audio bit depth */
        void *app_ud;                                   /* Application user data */
} dma_user_data_t;

typedef struct {
        sys_audio_path_t sys_audio_path;
        bool src_used;
} audio_path_t;

static bool pcm_loopback = false;
static sleep_mode_t pm_mode = pm_mode_active;
static  dma_user_data_t dma_user_data[CHANNEL_NUM_MAX * MAX_NO_PATHS * 2];
static audio_path_t audio_path[MAX_NO_PATHS];

static bool dev_type_out[AUDIO_NUM_DEVICES];
static bool dev_type_in[AUDIO_NUM_DEVICES];
static bool audio_path_idx_status[MAX_NO_PATHS];

static uint32_t get_sampling_rate(sys_audio_device_t *dev)
{
        uint32_t ret = 0;

        switch (dev->device_type) {
        case AUDIO_PDM:
                break;
        case AUDIO_PCM:
                ret = dev->pcm_param.sample_rate;
                break;
        case AUDIO_MEMORY:
                ret = dev->memory_param.sample_rate;
                break;
        default :
                ASSERT_ERROR(0);
                break;
        }

        return ret;
}

static bool is_src_conversion_required(sys_audio_device_t *dev_in, sys_audio_device_t *dev_out)
{
        if ((dev_in->device_type == AUDIO_PDM) || (dev_out->device_type == AUDIO_PDM)) {
                return true;
        }

        /* need SRC just to pass-through; PCM->RAM->PCM is separate use case */
        if ((dev_in->device_type == AUDIO_PCM) && (dev_out->device_type == AUDIO_PCM)) {
                return true;
        }

        if (get_sampling_rate(dev_in) == get_sampling_rate(dev_out)) {
            return false;
        }

        return true;
}

#ifdef OS_FREERTOS
__STATIC_INLINE resource_mask_t dma_resource_mask(HW_DMA_CHANNEL num)
{
        const resource_mask_t res_mask[] = {
                RES_MASK(RES_ID_DMA_CH0), RES_MASK(RES_ID_DMA_CH1),
                RES_MASK(RES_ID_DMA_CH2), RES_MASK(RES_ID_DMA_CH3),
                RES_MASK(RES_ID_DMA_CH4), RES_MASK(RES_ID_DMA_CH5),
                RES_MASK(RES_ID_DMA_CH6), RES_MASK(RES_ID_DMA_CH7)
        };

        return res_mask[num];
}

static void dma_resource_mng(bool acquire, sys_audio_device_t *dev_id)
{
        uint8_t idx = 0;

        while (idx < 2) {

                if (dev_id->memory_param.dma_channel[idx] != HW_DMA_CHANNEL_INVALID) {

                        if (acquire) {
                                resource_acquire(dma_resource_mask(dev_id->memory_param.dma_channel[idx]), RES_WAIT_FOREVER);
                        } else {
                                resource_release(dma_resource_mask(dev_id->memory_param.dma_channel[idx]));
                        }
                }
                idx++;
        }
}

static void apu_src_resource_mng(bool acquire, uint8_t idx)
{
        if (acquire) {
                /* Acquire SRC */
                resource_acquire(RES_MASK(RES_ID_APU_SRC), RES_WAIT_FOREVER);
                audio_path[idx].src_used = true;
        } else {
                /* Release SRC */
                resource_release(RES_MASK(RES_ID_APU_SRC));
                audio_path[idx].src_used = false;
        }
}
#else /* NO OS */
#define dma_resource_mng(acquire, dev_id)
#define apu_src_resource_mng(acquire, idx)
#endif /* OS_PRESENT */
bool sys_audio_mgr_default_pdm_data_init(sys_audio_device_t *dev_id, bool stereo, SYS_AUDIO_MGR_MODE mode)
{
        memset(dev_id, 0, sizeof(sys_audio_device_t));

        dev_id->device_type = AUDIO_PDM;
        dev_id->pdm_param.mode = mode;
        dev_id->pdm_param.clk_frequency = APU_DEFAULT_PDM_FREQUENCY;
        dev_id->pdm_param.channel = (stereo ? HW_PDM_CHANNEL_LR : HW_PDM_CHANNEL_R);
        dev_id->pdm_param.in_delay = HW_PDM_DI_NO_DELAY;
        dev_id->pdm_param.out_delay = HW_PDM_DO_NO_DELAY;

        return true;
}

bool sys_audio_mgr_default_pcm_data_init(sys_audio_device_t *dev_id, uint8_t channel_total,
        SYS_AUDIO_MGR_MODE mode, SYS_AUDIO_MGR_PCM_FORMATS format)
{
        ASSERT_ERROR(channel_total > 0 && channel_total <= 2);

        memset(dev_id, 0, sizeof(sys_audio_device_t));

        dev_id->device_type = AUDIO_PCM;
        dev_id->pcm_param.mode = mode;
        dev_id->pcm_param.format = format;
        dev_id->pcm_param.clock = HW_PCM_CLK_DIVN;
        dev_id->pcm_param.sample_rate = APU_DEFAULT_SAMPLING_RATE;

        dev_id->pcm_param.total_channel_num = channel_total;
        dev_id->pcm_param.channel_delay = 0;
        dev_id->pcm_param.bits_depth = APU_DEFAULT_BITS_DEPTH;
        dev_id->pcm_param.enable_dithering = true;
        dev_id->pcm_param.output_mode = APU_DEFAULT_OUTPUT_MODE;
        dev_id->pcm_param.cycle_per_bit = APU_DEFAULT_PCM_CYCLE_PER_BIT;

        /* Note: ignored, except for format==PCM_MODE */
        dev_id->pcm_param.fsc_delay = APU_DEFAULT_FSC_DELAY;

        dev_id->pcm_param.inverted_fsc_polarity = false;
        dev_id->pcm_param.inverted_clk_polarity = true;
        /* Use default fsc_length, remember that fsc length time need to lower than fsc_div time */
        if (channel_total > 1) {
                dev_id->pcm_param.fsc_length = dev_id->pcm_param.total_channel_num / 2;
        } else {
                dev_id->pcm_param.fsc_length = 0;
        }

        return true;
}

bool sys_audio_mgr_default_memory_data_init(sys_audio_device_t *dev_id, bool stereo,
        uint32_t buff_len, uint32_t buffer_len_cb,
        sys_audio_mgr_buffer_ready_cb cb, void *ud)
{
        ASSERT_ERROR(buff_len);
        ASSERT_ERROR(buffer_len_cb);
        ASSERT_ERROR(buff_len >= buffer_len_cb);
        ASSERT_ERROR((buff_len % buffer_len_cb) == 0); //If multi-buffered/circular operation, disallow wrapping

        memset(dev_id, 0, sizeof(sys_audio_device_t));

        dev_id->device_type = AUDIO_MEMORY;
        dev_id->memory_param.dma_channel[0] = HW_DMA_CHANNEL_INVALID;
        dev_id->memory_param.dma_channel[1] = HW_DMA_CHANNEL_INVALID;
        dev_id->memory_param.bits_depth = APU_DEFAULT_BITS_DEPTH;

        dev_id->memory_param.buff_addr[0] = 0;
        dev_id->memory_param.buff_addr[1] = 0;

        dev_id->memory_param.total_buffer_len = buff_len;
        dev_id->memory_param.buffer_len_cb = buffer_len_cb;

        dev_id->memory_param.cb = cb;
        dev_id->memory_param.app_ud = ud;
        dev_id->memory_param.stereo = stereo;
        dev_id->memory_param.sample_rate = APU_DEFAULT_SAMPLING_RATE;

        return true;
}

static void initialize_pdm_reg(uint8_t idx, sys_audio_pdm_specific_t *param, SYS_AUDIO_MGR_DIRECTION dir)
{
        const sys_audio_device_t* dev_in = audio_path[idx].sys_audio_path.dev_in;
        const sys_audio_device_t* dev_out = audio_path[idx].sys_audio_path.dev_out;

        /* Over-sampling ratio should be at least 64 times the sampling rate to
         * avoid degradation of the audio quality */
        const sys_audio_device_t *other_dev = ((dir == DIRECTION_INPUT) ? dev_out : dev_in);

        if (other_dev->device_type == AUDIO_MEMORY) {
                ASSERT_ERROR(param->clk_frequency >= (other_dev->memory_param.sample_rate * 64));
        }

        if (other_dev->device_type == AUDIO_PCM) {
                if (other_dev->pcm_param.sample_rate < 48000) {
                        /* Maximum SRC bandwidth is 24KHz */
                        ASSERT_ERROR(param->clk_frequency >= (other_dev->pcm_param.sample_rate * 64));
                } else {
                        ASSERT_ERROR(param->clk_frequency >= (48000 * 64));
                }
        }

        hw_pdm_request_clk(param->clk_frequency);
        if (dir == DIRECTION_INPUT) {
                hw_pdm_set_input_delay(param->in_delay);
                hw_pdm_set_in_channel_swap(!param->swap_channel);
        } else {
                hw_pdm_set_output_delay(param->out_delay);
                hw_pdm_set_out_channel_swap(param->swap_channel);
        }

        hw_pdm_set_mode(param->mode);
}

__STATIC_INLINE uint16_t calculate_fsc_div(sys_audio_pcm_specific_t *param)
{
        return (param->bits_depth * param->total_channel_num);
}


static void validate_pcm_cfg(sys_audio_pcm_specific_t *param)
{
        switch (param->format) {
        case PCM_MODE:
                break;
        case I2S_MODE:
                ASSERT_ERROR(param->total_channel_num == 2);
                ASSERT_ERROR(param->channel_delay == 0);
                break;
        case TDM_MODE:
                ASSERT_ERROR(param->total_channel_num == 2);
                break;
        case IOM2_MODE:
                ASSERT_ERROR(param->channel_delay == 0);
                break;
        default:
                ASSERT_ERROR(0);
                break;
        }
}

static void initialize_pcm_reg(sys_audio_pcm_specific_t *param)
{
        validate_pcm_cfg(param);

        hw_pcm_config_t config = {0};
        HW_PCM_CLK_GENERATION div = HW_PCM_CLK_GEN_FRACTIONAL;

        uint8_t fsc_length = param->fsc_length;
        config.gpio_output_mode = param->output_mode;

        if (param->mode == MODE_SLAVE) {
                config.pcm_mode = HW_PCM_MODE_SLAVE;
        } else {
                config.pcm_mode = HW_PCM_MODE_MASTER;
        }

        if (!param->enable_dithering) {
                div = HW_PCM_CLK_GEN_INTEGER_ONLY;
        }

        uint16_t fsc_div = calculate_fsc_div(param);

        switch (param->format) {
        case PCM_MODE:
                config.config_mode = HW_PCM_CONFIG_GENERIC_PCM_MODE;
                config.pcm_param.channel_delay = param->channel_delay;
                config.pcm_param.fsc_polarity = param->inverted_fsc_polarity;
                config.pcm_param.clock_polarity = param->inverted_clk_polarity;
                config.pcm_param.fsc_delay = param->fsc_delay;
                config.pcm_param.fsc_div = fsc_div;
                config.pcm_param.fsc_length = fsc_length;
                break;
        case I2S_MODE:
                fsc_length = param->bits_depth / 8;

                config.config_mode = HW_PCM_CONFIG_I2S_MODE;
                config.i2s_param.fsc_length = fsc_length;
                config.i2s_param.fsc_div = fsc_div;
                break;
        case IOM2_MODE:
                config.config_mode = HW_PCM_CONFIG_IOM_MODE;
                config.iom_param.fsc_div = fsc_div;
                config.iom_param.fsc_polarity = param->inverted_fsc_polarity;
                break;
        case TDM_MODE:
                fsc_length = (param->bits_depth / 8) + param->channel_delay;

                config.config_mode = HW_PCM_CONFIG_TDM_MODE;
                config.tdm_param.fsc_polarity = param->inverted_fsc_polarity;
                config.tdm_param.channel_delay = param->channel_delay;
                config.tdm_param.fsc_length = fsc_length;
                config.tdm_param.fsc_div = fsc_div;
                break;
        default:
                ASSERT_WARNING(0);
                break;
        }

        /* The FSC length must be smaller or equal to the bit_depth plus the channel offset,
         *  which is fsc_div
         */
        if (fsc_length > 0) {

                ASSERT_ERROR((fsc_length <= 8)  && fsc_length * 8 <= fsc_div - 8);

                if (param->cycle_per_bit) {
                        ASSERT_ERROR(fsc_div > CHANNEL_NUM_MAX * fsc_length);
                }
        }

        hw_pcm_init(&config);

        hw_pcm_init_clk_reg(param->clock, param->sample_rate/1000, param->bits_depth, param->total_channel_num, div);
}

static void dma_transfer_cb(void *user_data, dma_size_t len)
{
        dma_user_data_t *dma_user_data = (dma_user_data_t *)user_data;
        uint8_t bus_width = 1;

        /* Calculate index for the range DMA may now be recording to or playing back */
        if (dma_user_data->bit_depth > 16) {
                bus_width = 4;
        } else if (dma_user_data->bit_depth > 8) {
                bus_width = 2;
        }

        /* Calculate index for the range DMA may now be recording to or playing back in bytes*/
        uint32_t next_buff_len_pos = dma_user_data->buff_block.buff_len_pos + dma_user_data->buff_block.buff_len_cb;

        dma_user_data->buff_block.buff_len_pos = next_buff_len_pos;

        if (hw_dma_is_channel_active(dma_user_data->dma_channel_number)) {
                uint32_t num_of_transfers = ((next_buff_len_pos +
                        dma_user_data->buff_block.buff_len_cb) / bus_width) - 1;

                if (num_of_transfers > UINT16_MAX) {
                        num_of_transfers &= UINT16_MAX;
                }

                hw_dma_channel_update_int_ix(dma_user_data->dma_channel_number, num_of_transfers);
        } else if (next_buff_len_pos < dma_user_data->buff_block.buff_len_total) {

                uint32_t buf_len = (dma_user_data->buff_block.buff_len_total - next_buff_len_pos) /
                        bus_width;
                uint16_t num_of_transfers = dma_user_data->buff_block.buff_len_cb / bus_width - 1;
                uint32_t address = dma_user_data->buff_block.address + next_buff_len_pos;
                if (buf_len > UINT16_MAX + 1) {
                        buf_len = UINT16_MAX + 1;
                }

                if (num_of_transfers > UINT16_MAX) {
                        num_of_transfers = UINT16_MAX;
                }

                if (dma_user_data->dma_channel_number % 2 == 0) {
                        hw_dma_channel_update_destination(dma_user_data->dma_channel_number,
                                (void *)address,
                                buf_len,
                                dma_transfer_cb);
                } else {
                        hw_dma_channel_update_source(dma_user_data->dma_channel_number,
                                (void *)address,
                                buf_len,
                                dma_transfer_cb);
                }

                hw_dma_channel_update_int_ix(dma_user_data->dma_channel_number, num_of_transfers);
                hw_dma_channel_enable(dma_user_data->dma_channel_number, HW_DMA_STATE_ENABLED);
        }

        /* We might not have a call-back - if using two audio paths (e.g. PDM->RAM and RAM->PCM)
         * only one path's call-back is required to do any post-capture/pre-playback processing.
         *
         * If doing processing in IRQ context, the app call-back must maintain a read/write index,
         * and use buff_block.buff_len_cb both to update it and specify how much data to process.
         * It should reset it's index to 0 when >= buff_block.buff_len_total.
         *
         * If passing responsibility to a task with notify, the task should maintain a read/write index
         * and compare buff_block.buff_len_pos to calculate quantity to process. This way, multiple
         * IRQs and delayed notification handling will lead to it processing the available range of
         * data, not just a single chunk. It must cope with buff_len_pos having wrapped through 0.
         */
        if (dma_user_data->cb != NULL) {
                dma_user_data->cb(&(dma_user_data->buff_block), dma_user_data->app_ud);
        }
}

static void initialize_dma_reg(uint8_t idx, sys_audio_memory_specific_t *param, SYS_AUDIO_MGR_DMA_DIR dir)
{
        /* Setup generic left/right channel data parameter */

        /* Audio support up to two 32-bit channels */
        ASSERT_ERROR(param->bits_depth <= BIT_DEPTH_MAX);
        ASSERT_ERROR(param->buffer_len_cb );
        ASSERT_ERROR(param->total_buffer_len);
        ASSERT_ERROR(param->buffer_len_cb <= param->total_buffer_len);

        DMA_setup channel_setup;
        uint32_t offset = 0;
        sys_audio_path_t* const path = &audio_path[idx].sys_audio_path;

        channel_setup.circular = HW_DMA_MODE_NORMAL;

        if (param->bits_depth > 16) {
                channel_setup.bus_width = HW_DMA_BW_WORD;
        } else if (param->bits_depth > 8) {
                channel_setup.bus_width = HW_DMA_BW_HALFWORD;
                offset = 2;
        } else {
                channel_setup.bus_width = HW_DMA_BW_BYTE;
                offset = 3;
        }

        channel_setup.length = param->total_buffer_len >> (channel_setup.bus_width/2);
        channel_setup.irq_nr_of_trans = param->buffer_len_cb >> (channel_setup.bus_width/2);

        if (channel_setup.length > UINT16_MAX + 1) {
                channel_setup.length = UINT16_MAX + 1;
        }

        channel_setup.irq_enable = HW_DMA_IRQ_STATE_ENABLED;
        channel_setup.dreq_mode = HW_DMA_DREQ_TRIGGERED;
        channel_setup.burst_mode = HW_DMA_BURST_MODE_DISABLED;
        channel_setup.a_inc = (dir == DMA_DIR_MEM_TO_PERIPH) ? HW_DMA_AINC_TRUE : HW_DMA_AINC_FALSE;
        channel_setup.b_inc = (dir == DMA_DIR_MEM_TO_PERIPH) ? HW_DMA_BINC_FALSE : HW_DMA_BINC_TRUE;
        channel_setup.callback = dma_transfer_cb;
        channel_setup.dma_prio = HW_DMA_PRIO_2;
        channel_setup.dma_idle = HW_DMA_IDLE_INTERRUPTING_MODE;
        channel_setup.dma_init = HW_DMA_INIT_AX_BX_AY_BY;

        if (audio_path[idx].src_used) {
                channel_setup.dma_req_mux = HW_DMA_TRIG_SRC_RXTX;
        } else {
                channel_setup.dma_req_mux = HW_DMA_TRIG_PCM_RXTX;
        }

        /* support up to 2 channel */
        for (uint8_t ch = 0; ch < CHANNEL_NUM_MAX; ch++) {

                if (param->dma_channel[ch] != HW_DMA_CHANNEL_INVALID) {

                        channel_setup.channel_number = param->dma_channel[ch];

                        /* Decide data in/out location */
                        if (dir == DMA_DIR_MEM_TO_PERIPH) {
                                //Odd channels are only applicable for DMA_DIR_MEM_TO_PERIPH
                                ASSERT_ERROR((channel_setup.channel_number & 0x1) == 1);

                                channel_setup.src_address = param->buff_addr[ch];
                                if (audio_path[idx].src_used) {
                                        channel_setup.dest_address = (ch == 0) ?
                                                ((uint32_t)&(APU->SRC1_IN1_REG) + offset) :
                                                ((uint32_t)&(APU->SRC1_IN2_REG) + offset);
                                } else {
                                        channel_setup.dest_address = (ch == 0) ?
                                                ((uint32_t)&(APU->PCM1_OUT1_REG) + offset) :
                                                ((uint32_t)&(APU->PCM1_OUT2_REG) + offset);
                                }
                        } else {
                                //Even channels are only applicable for DMA_DIR_PERIPH_TO_MEM
                                ASSERT_ERROR((channel_setup.channel_number & 0x1) == 0);

                                channel_setup.dest_address = param->buff_addr[ch];
                                if (audio_path[idx].src_used) {
                                        channel_setup.src_address = (ch == 0) ?
                                                ((uint32_t)&(APU->SRC1_OUT1_REG) + offset) :
                                                ((uint32_t)&(APU->SRC1_OUT2_REG) + offset);
                                } else {
                                        channel_setup.src_address = (ch == 0) ?
                                                ((uint32_t)&(APU->PCM1_IN1_REG) + offset) :
                                                ((uint32_t)&(APU->PCM1_IN2_REG) + offset);
                                }
                        }

                        uint8_t i = ch | (dir << 1);

                        dma_user_data[i].dma_channel_number = param->dma_channel[ch];
                        dma_user_data[i].buff_block.buff_len_total = param->total_buffer_len;
                        dma_user_data[i].buff_block.buff_len_pos = 0;
                        dma_user_data[i].buff_block.buff_len_cb =  param->buffer_len_cb;
                        dma_user_data[i].buff_block.address = param->buff_addr[ch];
                        dma_user_data[i].cb = param->cb;
                        dma_user_data[i].path = path;
                        dma_user_data[i].app_ud = param->app_ud;
                        dma_user_data[i].buff_block.channel_num = ch;
                        dma_user_data[i].buff_block.stereo = param->stereo;
                        dma_user_data[i].bit_depth = param->bits_depth;

                        channel_setup.user_data = &dma_user_data[i];

                        hw_dma_channel_initialization(&channel_setup);
                }
        }
}

static void initialize_reg(uint8_t idx, sys_audio_device_t *dev_id, SYS_AUDIO_MGR_DIRECTION dir)
{
        switch (dev_id->device_type) {
        case AUDIO_PDM:
                initialize_pdm_reg(idx, &(dev_id->pdm_param), dir);
                break;
        case AUDIO_PCM:
                initialize_pcm_reg(&(dev_id->pcm_param));
                break;
        case AUDIO_MEMORY:
                dma_resource_mng(true, dev_id);
                initialize_dma_reg(idx, &(dev_id->memory_param), dir);
                break;
        default :
                ASSERT_ERROR(0);
                break;
        }
}

/*
 * \brief Start audio device input or output - helper function
 *
 * \param [in] dev input or output device
 * \return
 *         \retval true in case of closing audio manager with success
 *         \retval false in case of closing audio manager failed
 *
 */
static bool start_device(sys_audio_device_t *dev)
{
        bool ret = true;

        switch (dev->device_type) {
        case AUDIO_PDM:
                if (dev->pdm_param.mode == MODE_MASTER) {
                        hw_pdm_enable();
                        ret = hw_pdm_get_status();
                }
                break;
        case AUDIO_PCM:
                hw_pcm_enable();
                ret = hw_pcm_is_enabled();
                break;
        case AUDIO_MEMORY:
                for (uint8_t i = 0; i < CHANNEL_NUM_MAX; i++) {
                        if (dev->memory_param.dma_channel[i] != HW_DMA_CHANNEL_INVALID) {
                                hw_dma_channel_enable(dev->memory_param.dma_channel[i],
                                        HW_DMA_STATE_ENABLED);
                                ret = hw_dma_is_channel_active(dev->memory_param.dma_channel[i]);

                                if (ret == false) {
                                        break;
                                }
                        }
                }
                break;
        default :
                ASSERT_ERROR(0);
                break;
        }

        return ret;
}

static bool stop_device(sys_audio_device_t *dev)
{
        bool ret = true;

        switch (dev->device_type) {
        case AUDIO_PDM:
                hw_pdm_disable();

                if (dev->pdm_param.mode == MODE_MASTER) {
                        ret = !hw_pdm_get_status();
                }
                break;
        case AUDIO_PCM:
                hw_pcm_disable();
                ret = !hw_pcm_is_enabled();
                break;
        case AUDIO_MEMORY:
                for (uint8_t i = 0; i < CHANNEL_NUM_MAX; i++) {
                        if (dev->memory_param.dma_channel[i] != HW_DMA_CHANNEL_INVALID) {
                                hw_dma_channel_enable(dev->memory_param.dma_channel[i],
                                        HW_DMA_STATE_DISABLED);
                                ret = !hw_dma_is_channel_active(dev->memory_param.dma_channel[i]);

                                if (ret == false) {
                                        break;
                                }
                        }
                }
                break;
        default :
                ASSERT_ERROR(0);
                break;
        }

        return ret;
}

__STATIC_INLINE bool validate_path(sys_audio_device_t *dev_in,  sys_audio_device_t *dev_out)
{
        return (dev_in != NULL && dev_out != NULL &&
                dev_in->device_type != AUDIO_INVALID_DEVICE &&
                dev_out->device_type != AUDIO_INVALID_DEVICE);
}

static uint8_t get_path_idx(sys_audio_path_t* path)
{
        if (!path) {
                return MAX_NO_PATHS;
        }

        for (uint8_t idx = 0; idx < MAX_NO_PATHS; idx++) {
                if (audio_path_idx_status[idx] == true) {
                        if (path->dev_in == audio_path[idx].sys_audio_path.dev_in &&
                                path->dev_out == audio_path[idx].sys_audio_path.dev_out){
                                return idx;
                        }
                }
        }

        return MAX_NO_PATHS;
}

bool sys_audio_mgr_start(sys_audio_path_t* path)
{
        uint8_t idx = get_path_idx(path);

        if (idx == MAX_NO_PATHS) {
                return false;
        }

        if (!start_device(path->dev_in)) {
                return false;
        }

        if (!start_device(path->dev_out)) {
                stop_device(path->dev_in);
                return false;
        }

        if (audio_path[idx].src_used) {
                if (!hw_apu_src_get_status()) {
                        hw_apu_src_enable();

                        if (!hw_apu_src_get_status()) {

                                if (hw_apu_src_get_mode(DIRECTION_INPUT)) {
                                        stop_device(path->dev_in);
                                }
                                return false;
                        }
                }
        }

        return true;
}

bool sys_audio_mgr_stop(sys_audio_path_t* path)
{
        bool ret = false;
        uint8_t idx = get_path_idx(path);

        if (idx == MAX_NO_PATHS) {
                return false;
        }

        if (audio_path[idx].src_used) {

                if (hw_apu_src_get_status()) {
                        hw_apu_src_disable();

                        while (hw_apu_src_get_status());
                }
        }

        ret = stop_device(path->dev_in);

        if (ret) {
                ret = stop_device(path->dev_out);
        }

        return ret;
}

static void assert_src_pcm_mode(sys_audio_device_t *dev)
{
        /* In case of PCM/IOM MODE as input/output device bit depth should be equal to
         * 32 bits to be processed by SRC for 2 channels (left and right) (32 bits for each register) */
        if (dev->device_type == AUDIO_PCM ) {
                if (dev->pcm_param.format == PCM_MODE || dev->pcm_param.format == IOM2_MODE) {
                        if (dev->pcm_param.total_channel_num == 2) {
                                ASSERT_ERROR(dev->pcm_param.bits_depth == BIT_DEPTH_MAX);
                        }
                }
        }
}

__STATIC_INLINE HW_APU_SRC_SELECTION find_audio_lld_device(SYS_AUDIO_MGR_DEVICE dev)
{
        switch (dev) {
        case AUDIO_PCM:
                return HW_APU_SRC_PCM;
        case AUDIO_PDM:
                return HW_APU_SRC_PDM;
        case AUDIO_MEMORY:
                return HW_APU_SRC_REGS;
        case AUDIO_INVALID_DEVICE:
        case AUDIO_NUM_DEVICES:
        default:
                return HW_APU_SRC_INVALID_DEVICE;
        }
}

static void initialize_apu_src_reg(sys_audio_device_t *dev_in, sys_audio_device_t *dev_out)
{
        assert_src_pcm_mode(dev_in);
        assert_src_pcm_mode(dev_out);

        /* Initialize src */

        /* Select the input */
        hw_apu_src_select_input(find_audio_lld_device(dev_in->device_type));

        /* Set src clk at kHz divide Hz by 1000 */
        uint16_t src_clk = APU_DEFAULT_SRC_CLK / 1000;
        uint32_t in_sample_rate = 0;
        uint32_t out_sample_rate = 0;

        /* Interfaces with sample rate (PCM/MEMORY)
         * initialize the fsc and iir setting in src
         */
        if (dev_in->device_type != AUDIO_PDM) {
                in_sample_rate = get_sampling_rate(dev_in);
        }

        if (dev_out->device_type != AUDIO_PDM) {
                out_sample_rate = get_sampling_rate(dev_out);
        }

        hw_apu_src_init(src_clk, in_sample_rate, out_sample_rate);

        /* Only in case that interface is memory it is used the manual mode */
        if (dev_in->device_type != AUDIO_MEMORY) {
                hw_apu_src_set_automode(HW_APU_SRC_IN);
        } else {
                hw_apu_src_set_manual_mode(HW_APU_SRC_IN);
        }

        if (dev_out->device_type != AUDIO_MEMORY) {
                hw_apu_src_set_automode(HW_APU_SRC_OUT);
        } else {
                hw_apu_src_set_manual_mode(HW_APU_SRC_OUT);
        }

        /* Enable the SRC FIFO and set direction. FIFO cannot be enabled in stereo mode */
        if (dev_in->device_type == AUDIO_MEMORY &&
                dev_in->memory_param.stereo == false) {
                hw_apu_src_enable_fifo(HW_APU_SRC_IN);
        } else if (dev_out->device_type == AUDIO_MEMORY &&
                dev_out->memory_param.stereo == false) {
                hw_apu_src_enable_fifo(HW_APU_SRC_OUT);
        } else {
                hw_apu_src_disable_fifo();
        }
}

static void select_output(uint8_t idx, sys_audio_device_t *dev_out)
{
        switch (dev_out->device_type) {
        case AUDIO_PCM:
                ASSERT_ERROR(hw_pcm_get_pcm_input_mux() == HW_PCM_INPUT_MUX_OFF);
                hw_pdm_set_output_channel_config(HW_PDM_CHANNEL_NONE);
                if (audio_path[idx].src_used) {
                        hw_pcm_set_pcm_input_mux(HW_PCM_INPUT_MUX_SRC_OUT);
                } else {
                        hw_pcm_set_pcm_input_mux(HW_PCM_INPUT_MUX_PCM_OUT_REG);
                }
                break;
        case AUDIO_PDM:
                hw_pdm_set_output_channel_config(dev_out->pdm_param.channel);
                hw_pcm_set_pcm_input_mux(HW_PCM_INPUT_MUX_OFF);
                break;
        case AUDIO_MEMORY:
                if (audio_path[idx].src_used) {
                        hw_pdm_set_output_channel_config(HW_PDM_CHANNEL_NONE);
                }
                hw_pcm_set_pcm_input_mux(HW_PCM_INPUT_MUX_OFF);
                break;
        default:
                ASSERT_WARNING(0);
                break;
        }
}

static void assert_audio_mem_channels_consistency(sys_audio_device_t *dev_in,  sys_audio_device_t *dev_out)
{
        sys_audio_device_t *pcm_dev = NULL;
        sys_audio_device_t *mem_dev = NULL;

        if (dev_in->device_type == AUDIO_PCM && dev_out->device_type == AUDIO_MEMORY) {
                pcm_dev = dev_in;
                mem_dev = dev_out;
        } else if (dev_in->device_type == AUDIO_MEMORY && dev_out->device_type == AUDIO_PCM) {
                pcm_dev = dev_out;
                mem_dev = dev_in;
        } else {
                return;
        }

        switch (pcm_dev->pcm_param.format) {
        case PCM_MODE:
        case IOM2_MODE:
                /* Mono mode */
                if ((pcm_dev->pcm_param.total_channel_num == 1)) {
                        ASSERT_ERROR(mem_dev->memory_param.stereo == false);
                } else {
                        /* Stereo mode*/
                        ASSERT_ERROR(mem_dev->memory_param.stereo == true);
                }
                break;
        case I2S_MODE:
        case TDM_MODE:
                ASSERT_ERROR(mem_dev->memory_param.stereo == true);
                break;
        default:
                break;
        }
}

static void validate_cfg(sys_audio_device_t *dev_in, sys_audio_device_t *dev_out)
{

        /* Check current path's input and output devices */

        /* Check validity of input and output device of the current path */
        ASSERT_ERROR(validate_path(dev_in, dev_out));

        if (dev_in->device_type == dev_out->device_type) {
                /* Check current use of PDM */
                ASSERT_ERROR(dev_in->device_type != AUDIO_PDM);
        }

        /* Check previous use of devices */
        /*
         * According to Audio Unit Block Diagram each interface (PCM/PDM),
         * can be input for both SRCs but only one can be as output except from memory.
         * For that case, a table that matches
         * each device_type used in output for each data path implemented as mask.
         */

        /* PDM may only be used once, either as input or as an output device */
        /* Check previous use of PDM */
        if (dev_in->device_type == AUDIO_PDM) {
                ASSERT_ERROR(!dev_type_out[AUDIO_PDM]);
        }

        if (dev_out->device_type == AUDIO_PDM) {
                ASSERT_ERROR(!dev_type_in[AUDIO_PDM]);
        }

        /* Check that the audio channels correspond to the number of memory channels */
        assert_audio_mem_channels_consistency(dev_in, dev_out);
}

static bool deep_copy_audio_paths(sys_audio_device_t *udev_in, sys_audio_device_t *udev_out, uint8_t idx)
{
        sys_audio_path_t* const path = &audio_path[idx].sys_audio_path;

#  ifdef OS_FREERTOS
        if (2 * sizeof(sys_audio_device_t) > OS_GET_FREE_HEAP_SIZE()) {
                path->dev_in = NULL;
                path->dev_out = NULL;
                return false;
        }
#  endif

        path->dev_in = AUDIO_MALLOC(sizeof(sys_audio_device_t));

        if (!path->dev_in) {
                AUDIO_FREE(path->dev_in);

                return false;
        }

        path->dev_out = AUDIO_MALLOC(sizeof(sys_audio_device_t));
        if (!path->dev_out) {
                AUDIO_FREE(path->dev_in);
                AUDIO_FREE(path->dev_out);
                return false;
        }

        *(path->dev_in) = *(udev_in);
        *(path->dev_out) = *(udev_out);

        return true;
}

static void check_cfgs(sys_audio_device_t *dev1, sys_audio_device_t *dev2)
{
        switch (dev1->device_type) {
        case AUDIO_PCM:
                ASSERT_ERROR(dev1->pcm_param.bits_depth == dev2->pcm_param.bits_depth);
                ASSERT_ERROR(dev1->pcm_param.sample_rate == dev2->pcm_param.sample_rate);
                ASSERT_ERROR(dev1->pcm_param.total_channel_num == dev2->pcm_param.total_channel_num);
                ASSERT_ERROR(dev1->pcm_param.channel_delay == dev2->pcm_param.channel_delay);
                ASSERT_ERROR(dev1->pcm_param.enable_dithering == dev2->pcm_param.enable_dithering);
                ASSERT_ERROR(dev1->pcm_param.clock == dev2->pcm_param.clock);
                ASSERT_ERROR(dev1->pcm_param.cycle_per_bit == dev2->pcm_param.cycle_per_bit);
                ASSERT_ERROR(dev1->pcm_param.format == dev2->pcm_param.format);
                ASSERT_ERROR(dev1->pcm_param.fsc_delay == dev2->pcm_param.fsc_delay);
                ASSERT_ERROR(dev1->pcm_param.fsc_length == dev2->pcm_param.fsc_length);
                ASSERT_ERROR(dev1->pcm_param.inverted_clk_polarity == dev2->pcm_param.inverted_clk_polarity);
                ASSERT_ERROR(dev1->pcm_param.inverted_fsc_polarity == dev2->pcm_param.inverted_fsc_polarity);
                ASSERT_ERROR(dev1->pcm_param.mode == dev2->pcm_param.mode);
                ASSERT_ERROR(dev1->pcm_param.output_mode == dev2->pcm_param.output_mode);
                break;
        case AUDIO_PDM:
                ASSERT_ERROR(dev1->pdm_param.channel == dev2->pdm_param.channel);
                ASSERT_ERROR(dev1->pdm_param.clk_frequency == dev2->pdm_param.clk_frequency);
                ASSERT_ERROR(dev1->pdm_param.in_delay == dev2->pdm_param.in_delay);
                ASSERT_ERROR(dev1->pdm_param.mode == dev2->pdm_param.mode);
                ASSERT_ERROR(dev1->pdm_param.out_delay == dev2->pdm_param.out_delay);
                ASSERT_ERROR(dev1->pdm_param.swap_channel == dev2->pdm_param.swap_channel);
                break;
        default:
                ASSERT_ERROR(0);
                break;
        }
}

static uint8_t assert_same_cfg(sys_audio_device_t *dev)
{
        uint8_t i;

        /* Find the cfg of the device in all data paths */
        for (i = 0; i < MAX_NO_PATHS; i++) {
                if (audio_path[i].sys_audio_path.dev_in->device_type == dev->device_type) {
                        check_cfgs(audio_path[i].sys_audio_path.dev_in, dev);
                        return i;
                }

                if (audio_path[i].sys_audio_path.dev_out->device_type == dev->device_type) {
                        check_cfgs(audio_path[i].sys_audio_path.dev_out, dev);
                        return i;
                }
        }
        return i;
}

static uint8_t path_idx_acquire(sys_audio_device_t* dev_in, sys_audio_device_t* dev_out)
{
        uint8_t path_idx;

        for (path_idx = 0; path_idx < MAX_NO_PATHS; path_idx++) {
                if (audio_path_idx_status[path_idx] == false) {
                        audio_path_idx_status[path_idx] = true;
                        return path_idx;
                }
        }

        /* Each device type can be used multiple times as input device
         * but with the same configuration (except from memory)
         */
        if (dev_type_in[dev_in->device_type] && dev_in->device_type != AUDIO_MEMORY) {
                /* Find the cfg of the device in all data paths */
                return (assert_same_cfg(dev_in));
        }

        /* Each device type (except from memory) should only be used once as output device */
        if (dev_type_out[dev_out->device_type] && dev_out->device_type != AUDIO_MEMORY) {
                /* Find the cfg of the device in all data paths */
                return (assert_same_cfg(dev_out));
        }

        return path_idx;
}

sys_audio_path_t* sys_audio_mgr_open(sys_audio_device_t *dev_id_in, sys_audio_device_t *dev_id_out)
{
        /* Input validation */
        validate_cfg(dev_id_in, dev_id_out);

        uint8_t idx = path_idx_acquire(dev_id_in, dev_id_out);

        ASSERT_ERROR(idx < MAX_NO_PATHS);

        /* Deep copy of system path cfg */
        ASSERT_ERROR(deep_copy_audio_paths(dev_id_in, dev_id_out, idx));

        sys_audio_path_t* const path = &audio_path[idx].sys_audio_path;
        sys_audio_device_t* const dev_in = audio_path[idx].sys_audio_path.dev_in;
        sys_audio_device_t* const dev_out = audio_path[idx].sys_audio_path.dev_out;

#ifdef OS_FREERTOS
        uint8_t i;
        for (i = 0; i < MAX_NO_PATHS; i++) {
                /* Disable interrupt in case of PCM1 input to PCM1 output */
                if (dev_in->device_type == AUDIO_PCM && dev_out->device_type == AUDIO_PCM) {
                        /* Set sleep mode to active */
                        pm_mode = pm_mode_active;
                        pcm_loopback = true;
                        break;
                }
        }

        if (i == MAX_NO_PATHS) {
                /* Set sleep mode to idle */
                pm_mode = pm_mode_idle;
        }

        pm_sleep_mode_request(pm_mode);
#endif /* OS_FREERTOS */

        if (is_src_conversion_required(dev_in, dev_out)) {
                /* Acquire SRC and define src_config for each path*/
                apu_src_resource_mng(true, idx);
                /* SRC configuration */
                initialize_apu_src_reg(dev_in, dev_out);
        } else {
                /* Data path with input and output Audio Memory is supported only with the use of SRC */
                if (dev_in->device_type == dev_out->device_type) {
                        ASSERT_ERROR(dev_in->device_type != AUDIO_MEMORY);
                }
        }

        /* Select the output */
        select_output(idx, dev_out);

        /* Input/Ouput interface configuration for each data path */
        /* Check if the device input (except memory) is already used in previous paths,
         * if not then initialize the device
         * */
        if (dev_in->device_type == AUDIO_MEMORY ||
                (dev_in->device_type != AUDIO_MEMORY && !dev_type_in[dev_in->device_type])) {
                initialize_reg(idx, dev_in, DIRECTION_INPUT);
        }
        if (dev_out->device_type == AUDIO_MEMORY ||
                (dev_out->device_type != AUDIO_MEMORY && !dev_type_out[dev_out->device_type])) {
                initialize_reg(idx, dev_out, DIRECTION_OUTPUT);
        }
        dev_type_in[dev_in->device_type] = true;
        dev_type_out[dev_out->device_type] = true;

        return path;
}


static void close_device(sys_audio_device_t *dev)
{
        if (dev->device_type == AUDIO_MEMORY) {
                dma_resource_mng(false, dev);
        }

        AUDIO_FREE(dev);
}

static bool is_any_path_active(void)
{
        /* Check if there is any src active */
        for (uint8_t idx = 0; idx < MAX_NO_PATHS; idx++) {
                if (audio_path[idx].src_used) {
                        return true;
                }
        }
        /* Check if there is any device active except from memory */
        for (SYS_AUDIO_MGR_DEVICE dev = AUDIO_PCM; dev < AUDIO_NUM_DEVICES; dev++) {
                if (dev != AUDIO_MEMORY && dev_type_in[dev]) {
                        return true;
                }
        }

        return false;
}

bool sys_audio_mgr_close(sys_audio_path_t* path)
{
        uint8_t idx = get_path_idx(path);

        if (idx == MAX_NO_PATHS) {
                return false;
        }

        close_device(path->dev_in);

        if (audio_path[idx].src_used) {
                apu_src_resource_mng(false, idx);
        }

        close_device(path->dev_out);

        dev_type_out[path->dev_out->device_type] = false;
        dev_type_in[path->dev_in->device_type] = false;

        if (is_any_path_active() == false) {
#ifdef OS_FREERTOS
                pm_sleep_mode_release(pm_mode);
#endif /* OS_FREERTOS */
                /* Initialize device types */
                memset(dev_type_out, false, AUDIO_NUM_DEVICES);
                memset(dev_type_in, false, AUDIO_NUM_DEVICES);
        }

        /* Remove used audio data path */
        audio_path_idx_status[idx] = false;

        return true;
}
#endif /* #if dg_configSYS_AUDIO_MGR */
