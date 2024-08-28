/**
 ****************************************************************************************
 *
 * @file audio_tack.c
 *
 * @brief Audio task
 *
 * Copyright (C) 2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include "audio_task.h"
#if (DEMO_MEM_TO_MEM || DEMO_PCM_RECORD_PLAYBACK || DEMO_PDM_RECORD_PLAYBACK)
#include "periph_setup.h"
#endif
#if DEMO_MEM_TO_MEM
#include "demo_helpers.h"
#endif

#define CHANNEL_NUM_MAX          2                      /* the max number of audio channels */

const uint32_t flash_memory_channel_addr[2] = {0x0, NVMS_LOG_PART_SIZE / 2};
context_demo_apu_t context_demo_apu;

#if (DEMO_MEM_TO_MEM || DEMO_PCM_RECORD_PLAYBACK || DEMO_PDM_RECORD_PLAYBACK)
static void audio_buffer_ready_cb(sys_audio_mgr_buffer_data_block_t *buffer, void *app_ud)
{
        context_demo_apu_t *demo_apu = app_ud;

        demo_apu->available_to_read += buffer->buff_len_cb;
        if (demo_apu->available_to_read == CHANNEL_NUM_MAX * buffer->buff_len_total) {
                context_demo_apu.available_to_read = 0;
                OS_TASK_NOTIFY_FROM_ISR(demo_apu->audio_task, 1 << 0, OS_NOTIFY_NO_ACTION);
        }
}
# if DEMO_MEM_TO_MEM
static void audio_buffer_ready_cb_2(sys_audio_mgr_buffer_data_block_t *buff_data_block, void *app_ud)
{
        context_demo_apu_t *demo_apu = app_ud;

        demo_apu->available_to_read_2 += buff_data_block->buff_len_cb;

        if (demo_apu->available_to_read_2 == CHANNEL_NUM_MAX * buff_data_block->buff_len_total) {
                context_demo_apu.available_to_read_2 = 0;
                OS_TASK_NOTIFY_FROM_ISR(demo_apu->audio_task, 1 << 1, OS_NOTIFY_SET_BITS);
        }
}
# endif
#endif

static void audio_mgr_start()
{
        bool retval;

        printf("\n\r\n\r>>> Selected configuration is started. <<<\n\r");
        /* Open audio interfaces of apu for the required path */
        context_demo_apu.audio_path = sys_audio_mgr_open(&context_demo_apu.dev_in,
                &context_demo_apu.dev_out);
        OS_ASSERT(context_demo_apu.audio_path);
        printf_settings(&context_demo_apu.dev_in, INPUT_DEVICE);
        printf_settings(&context_demo_apu.dev_out, OUTPUT_DEVICE);
        /* Enable devices of the required path */
        retval = sys_audio_mgr_start(context_demo_apu.audio_path);
        OS_ASSERT(retval);
}

static void audio_mgr_stop()
{
        printf("\n\r\n\r>>> Selected stop <<<\n\r");
        sys_audio_mgr_stop(context_demo_apu.audio_path);
        printf("\n\r\n\r>>> Selected close <<<\n\r");
        sys_audio_mgr_close(context_demo_apu.audio_path);
}

#if (DEMO_MEM_TO_MEM)
void audio_task( void *pvParameters )
{
        bool retval_in, retval_out;
        uint32_t size = DEMO_CHANNEL_DATA_BUF_TOTAL_SIZE;
        /* Initialize default parameters for MEMORY input */
        retval_in = sys_audio_mgr_default_memory_data_init(&context_demo_apu.dev_in, true, size,
                DEMO_CHANNEL_DATA_BUF_CB_SIZE,
                audio_buffer_ready_cb,
                &context_demo_apu);
        OS_ASSERT(retval_in);
        /* Overwrite default init values */
        context_demo_apu.dev_in.memory_param.sample_rate = SAMPLE_RATE_2;
        context_demo_apu.dev_in.memory_param.bits_depth = BIT_DEPTH;
        // Channels 1, 3, 5 or 7 must be used for SRC input
        context_demo_apu.dev_in.memory_param.dma_channel[0] = HW_DMA_CHANNEL_3;
        context_demo_apu.dev_in.memory_param.dma_channel[1] = HW_DMA_CHANNEL_1;

        /* Initialize default parameters for MEMORY output */
        retval_out = sys_audio_mgr_default_memory_data_init(&context_demo_apu.dev_out, true, size,
                DEMO_CHANNEL_DATA_BUF_CB_SIZE,
                audio_buffer_ready_cb_2,
                &context_demo_apu);
        OS_ASSERT(retval_out);
        /* Overwrite default init values */
        context_demo_apu.dev_out.memory_param.sample_rate = SAMPLE_RATE_1;
        context_demo_apu.dev_out.memory_param.bits_depth = BIT_DEPTH;
        // Channels 0, 2, 4 or 6 must be used for SRC output
        context_demo_apu.dev_out.memory_param.dma_channel[0] = HW_DMA_CHANNEL_2;
        context_demo_apu.dev_out.memory_param.dma_channel[1] = HW_DMA_CHANNEL_0;

        size = context_demo_apu.dev_in.memory_param.total_buffer_len *
                (context_demo_apu.dev_in.memory_param.bits_depth / 8);

        for (uint8_t i = 0 ; i < (context_demo_apu.dev_in.memory_param.stereo ? 2 : 1); i++) {
                /* Configuration of DMA is needed when memory interface is used */
                if (context_demo_apu.dev_in.memory_param.dma_channel[i] != HW_DMA_CHANNEL_INVALID) {

                        if (size > OS_GET_FREE_HEAP_SIZE()) {
                                printf("No enough heap size %ld, reduce num of channel or memory buffer", size);
                                context_demo_apu.dev_in.memory_param.buff_addr[i] = 0;
                                return;
                        }
                        context_demo_apu.dev_in.memory_param.buff_addr[i] = (uint32_t)OS_MALLOC(size);

                        if (!context_demo_apu.dev_in.memory_param.buff_addr[i]) {
                                ASSERT_ERROR(0);
                        }
# if (SIN_DATA_SR_8K_F_1K || SIN_DATA_SR_96K_F_1K || SIN_DATA_SR_192K_F_12K || SIN_DATA_SR_192K_F_19_2K || SIN_DATA_SR_8K_TO_96K_F_1K)
                        demo_set_sinusoidal_pattern((uint32_t*)context_demo_apu.dev_in.memory_param.buff_addr[i],
                                context_demo_apu.dev_in.memory_param.total_buffer_len,
                                pcm_sin_data, ARRAY_LENGTH(pcm_sin_data),
                                context_demo_apu.dev_in.memory_param.sample_rate,
                                SIGNAL_INPUT_FREQ,
                                context_demo_apu.dev_in.memory_param.bits_depth);
# elif  (COS_DATA_SR_8K_F_1K || COS_DATA_SR_192K_F_12K || COS_DATA_SR_192K_F_19_2K)
                        demo_set_sinusoidal_pattern((uint32_t*)context_demo_apu.dev_in.memory_param.buff_addr[i],
                                context_demo_apu.dev_in.memory_param.total_buffer_len,
                                pcm_cos_data, ARRAY_LENGTH(pcm_cos_data),
                                context_demo_apu.dev_in.memory_param.sample_rate,
                                SIGNAL_INPUT_FREQ,
                                context_demo_apu.dev_in.memory_param.bits_depth);
# endif
                }

                /* Configuration of DMA is needed when memory interface is used */
                if (context_demo_apu.dev_out.memory_param.dma_channel[i] != HW_DMA_CHANNEL_INVALID) {

                        if (size > OS_GET_FREE_HEAP_SIZE()) {
                                printf("No enough heap size %ld, reduce num of channel or memory buffer", size);
                                context_demo_apu.dev_out.memory_param.buff_addr[i] = 0;
                                return;
                        }
                        context_demo_apu.dev_out.memory_param.buff_addr[i] = (uint32_t)OS_MALLOC(size);

                        if (!context_demo_apu.dev_out.memory_param.buff_addr[i]) {
                                ASSERT_ERROR(0);
                                return;
                        }
                }
        }

        uint32_t notified_value = 0;

        for ( ;; ) {
                if (!hw_gpio_get_pin_status(BTN_PIN)) {
                        audio_mgr_start();

                        while (notified_value != 1 << 1) {
                                /*
                                 * Wait on any of the event group bits, then clear them all
                                 */
                                OS_BASE_TYPE xResult = OS_TASK_NOTIFY_WAIT(0x0, OS_TASK_NOTIFY_ALL_BITS,
                                        &notified_value, OS_TASK_NOTIFY_FOREVER);

                                OS_ASSERT(xResult == OS_OK);

                                /* Check if the data are received */
                                if (notified_value & 1 << 1) {
                                        audio_mgr_stop();
                                }
                        }
                        notified_value = 0x00;
                }
        }
}
#elif (DEMO_PDM_MIC || DEMO_PCM_MIC)
void audio_task( void *pvParameters )
{
        bool retval_in, retval_out;

#if DEMO_PDM_MIC
        /* Initialize default parameters for PDM input */
        retval_in = sys_audio_mgr_default_pdm_data_init(&context_demo_apu.dev_in, false,
                MODE_MASTER);
        OS_ASSERT(retval_in);
        /* Overwrite default init values */
        context_demo_apu.dev_in.pdm_param.clk_frequency = PDM_FREQ;
#elif DEMO_PCM_MIC
        /* Initialize default parameters for I2S input */
        retval_in = sys_audio_mgr_default_pcm_data_init(&context_demo_apu.dev_in, CHANNEL_NUM_MAX, MODE_MASTER,
                I2S_MODE);

        OS_ASSERT(retval_in);
        /* Overwrite default init values */
        context_demo_apu.dev_in.pcm_param.sample_rate = SAMPLE_RATE_1;
        context_demo_apu.dev_in.pcm_param.bits_depth = BIT_DEPTH;
# if SYS_CLK_DIV1
        context_demo_apu.dev_in.pcm_param.clock = HW_PCM_CLK_DIV1;
# endif
#endif

        /* Initialize default parameters for I2S output */
        retval_out = sys_audio_mgr_default_pcm_data_init(&context_demo_apu.dev_out, CHANNEL_NUM_MAX, MODE_MASTER,
                I2S_MODE);

        OS_ASSERT(retval_out);
        /* Overwrite default init values */
        context_demo_apu.dev_out.pcm_param.sample_rate = SAMPLE_RATE_1;
        context_demo_apu.dev_out.pcm_param.bits_depth = BIT_DEPTH;
# if SYS_CLK_DIV1
        context_demo_apu.dev_out.pcm_param.clock = HW_PCM_CLK_DIV1;
# endif

        audio_mgr_start();

        OS_DELAY_MS(10000);

        audio_mgr_stop();
}

#elif (DEMO_PDM_RECORD_PLAYBACK || DEMO_PCM_RECORD_PLAYBACK)

static void  copy_ram_pattern_to_qspi()
{
        bool success = true;
        uint8_t i;
        uint32_t buff_size = DEMO_CHANNEL_DATA_BUF_TOTAL_SIZE;
        nvms_t part = ad_nvms_open(NVMS_LOG_PART);

        for (i = 0 ; i < (context_demo_apu.dev_out.memory_param.stereo ? 2 : 1); i++) {
                if (context_demo_apu.dev_out.memory_param.dma_channel[i] != HW_DMA_CHANNEL_INVALID) {
                        uint32_t flash_memory_addr = flash_memory_channel_addr[i];

                        /* addr is any address in partition address space
                         * buf can be any address in memory including QSPI mapped flash */
                        ad_nvms_write(part, flash_memory_addr,
                                (uint8_t *)context_demo_apu.dev_out.memory_param.buff_addr[i],
                                buff_size);

                        flash_memory_addr += (NVMS_LOG_PART_START + MEMORY_QSPIF_S_BASE);
                        if (memcmp((uint8_t *)context_demo_apu.dev_out.memory_param.buff_addr[i],
                                (uint8_t *)flash_memory_addr, buff_size)) {
                                printf("wrong write at addr : %ld\n\r", flash_memory_addr);
                                success = false;
                        }

                        if (context_demo_apu.dev_out.memory_param.buff_addr[i]) {
                                OS_FREE((void *)context_demo_apu.dev_out.memory_param.buff_addr[i]);
                        }
                }
        }

        printf("\r\nWrite with : %s\r\n", (success ? "Success" : "Failure"));
}

static void mic_record_init()
{
        uint8_t  i;
        bool retval_in, retval_out;
        uint32_t size = DEMO_CHANNEL_DATA_BUF_TOTAL_SIZE;

#if DEMO_PCM_RECORD_PLAYBACK
        /* Initialize default parameters for I2S input */
        retval_in = sys_audio_mgr_default_pcm_data_init(&context_demo_apu.dev_in, CHANNEL_NUM_MAX,
                MODE_MASTER, I2S_MODE);

        OS_ASSERT(retval_in);
        /* Overwrite default init values */
        context_demo_apu.dev_in.pcm_param.sample_rate = SAMPLE_RATE_1;
        context_demo_apu.dev_in.pcm_param.bits_depth = BIT_DEPTH;
# if SYS_CLK_DIV1
        context_demo_apu.dev_in.pcm_param.clock = HW_PCM_CLK_DIV1;
# endif
#elif DEMO_PDM_RECORD_PLAYBACK
        /* Initialize default parameters for PDM input */
        retval_in = sys_audio_mgr_default_pdm_data_init(&context_demo_apu.dev_in, true,
                MODE_MASTER);
        OS_ASSERT(retval_in);
        /* Overwrite default init values */
        context_demo_apu.dev_in.pdm_param.clk_frequency = PDM_FREQ;
#endif

        /* Initialize default parameters for memory output */
        retval_out = sys_audio_mgr_default_memory_data_init(&context_demo_apu.dev_out, true, size,
                DEMO_CHANNEL_DATA_BUF_CB_SIZE,
                audio_buffer_ready_cb,
                &context_demo_apu);
        OS_ASSERT(retval_out);
        /* Overwrite default init values */
        context_demo_apu.dev_out.memory_param.sample_rate = SAMPLE_RATE_2;
        context_demo_apu.dev_out.memory_param.bits_depth = BIT_DEPTH;

        // Channels 0, 2, 4 or 6 must be used for SRC output
        context_demo_apu.dev_out.memory_param.dma_channel[0] = HW_DMA_CHANNEL_2;
        context_demo_apu.dev_out.memory_param.dma_channel[1] = HW_DMA_CHANNEL_0;

        for (i = 0 ; i < (context_demo_apu.dev_out.memory_param.stereo ? 2 : 1); i++) {
                /* Configuration of DMA is needed when memory interface is used */
                if (context_demo_apu.dev_out.memory_param.dma_channel[i] != HW_DMA_CHANNEL_INVALID) {
                        if (size > OS_GET_FREE_HEAP_SIZE()) {
                                printf("No enough heap size %ld, reduce num of channel or memory buffer", size);
                                context_demo_apu.dev_out.memory_param.buff_addr[i] = 0;
                                return;
                        }
                        context_demo_apu.dev_out.memory_param.buff_addr[i] = (uint32_t)OS_MALLOC(size);
                }
        }
}

static void mic_playback_init()
{
        uint8_t i;
        bool retval_in, retval_out;
        uint32_t size = DEMO_CHANNEL_DATA_BUF_TOTAL_SIZE;

        retval_in = sys_audio_mgr_default_memory_data_init(&context_demo_apu.dev_in, true,
                size, DEMO_CHANNEL_DATA_BUF_CB_SIZE,
                audio_buffer_ready_cb,
                &context_demo_apu);

        OS_ASSERT(retval_in);
        /* Overwrite default init values */
        context_demo_apu.dev_in.memory_param.sample_rate = SAMPLE_RATE_2;
        context_demo_apu.dev_in.memory_param.bits_depth = BIT_DEPTH;

        // Channels 1, 3, 5 or 7 must be used for SRC input
        context_demo_apu.dev_in.memory_param.dma_channel[0] = HW_DMA_CHANNEL_3;
        context_demo_apu.dev_in.memory_param.dma_channel[1] = HW_DMA_CHANNEL_1;

        for (i = 0 ; i < (context_demo_apu.dev_in.memory_param.stereo ? 2 : 1); i++) {
                context_demo_apu.dev_in.memory_param.buff_addr[i] = flash_memory_channel_addr[i] +
                        NVMS_LOG_PART_START +
                        MEMORY_QSPIF_S_BASE;
        }

        /* Initialize default parameters for I2S output */
        retval_out = sys_audio_mgr_default_pcm_data_init(&context_demo_apu.dev_out, CHANNEL_NUM_MAX,
                MODE_MASTER, I2S_MODE);
        OS_ASSERT(retval_out);
        /* Overwrite default init values */
        context_demo_apu.dev_out.pcm_param.sample_rate = SAMPLE_RATE_1;
        context_demo_apu.dev_out.pcm_param.bits_depth = BIT_DEPTH;
#if SYS_CLK_DIV1
        context_demo_apu.dev_out.pcm_param.clock = HW_PCM_CLK_DIV1;
#endif

}

void audio_task( void *pvParameters )
{
        uint32_t notified_value = 0;
        for ( ;; ) {
                if (!hw_gpio_get_pin_status(BTN_PIN)) {
                        mic_record_init();
                        audio_mgr_start();
                        /*
                         * Wait on any of the event group bits, then clear them all
                         */
                        OS_BASE_TYPE xResult = OS_TASK_NOTIFY_WAIT(0x0, OS_TASK_NOTIFY_ALL_BITS, &notified_value,
                                OS_TASK_NOTIFY_FOREVER);
                        OS_ASSERT(xResult == OS_OK);

                        audio_mgr_stop();

                        /* Used only in DEMO_PDM_RECORD_PLAYBACK and DEMO_PCM_RECORD_PLAYBACK */
                        copy_ram_pattern_to_qspi();

                        mic_playback_init();
                        audio_mgr_start();

                        xResult = OS_TASK_NOTIFY_WAIT(0x0, OS_TASK_NOTIFY_ALL_BITS, &notified_value,
                                OS_TASK_NOTIFY_FOREVER);
                        OS_ASSERT(xResult == OS_OK);

                        audio_mgr_stop();
                }
        }
}
#endif

