/**
 ****************************************************************************************
 *
 * @file audio_task.h
 *
 * @brief Audio task
 *
 * Copyright (C) 2021-2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */
#ifndef INCLUDES_AUDIO_TASK_H_
#define INCLUDES_AUDIO_TASK_H_

#include "osal.h"
#include "sys_audio_mgr.h"

#define DEMO_PDM_MIC                    (0)
#define DEMO_PDM_RECORD_PLAYBACK        (0)
#define DEMO_PCM_MIC                    (0)
#define DEMO_PCM_RECORD_PLAYBACK        (1)
#define DEMO_MEM_TO_MEM                 (0)

/* Set the system clock to be used for PCM. If SYS_CLK_DIV1 sys clock is PLL96 for DA1469X */
#define SYS_CLK_DIV1                            (0)

/* Set the sample rate pair to be used for the conversion */
#define SR1_16_SR2_8                            (1)
#define SR1_48_SR2_32                           (0)

#if !(SR1_16_SR2_8 ^ SR1_48_SR2_32)
#error "Only one setup should be selected!!!"
#endif

#if SR1_16_SR2_8
#define SAMPLE_RATE_1   (16000)   /* Sample rate of PCM at 16 Khz */
#define SAMPLE_RATE_2   (8000)    /* Sample rate of MEMORY at 8 Khz */
#elif SR1_48_SR2_32
#define SAMPLE_RATE_1   (48000)   /* Sample rate of PCM at 48 Khz */
#define SAMPLE_RATE_2   (32000)    /* Sample rate of MEMORY at 32 Khz */
#endif

#define BIT_DEPTH_16    (1)
#define BIT_DEPTH_32    (0)

#if !(BIT_DEPTH_16 ^ BIT_DEPTH_32)
#error "Only one setup should be selected!!!"
#endif

#if BIT_DEPTH_16
#define BIT_DEPTH         (16)      /* Bit depth at 16 */
#elif BIT_DEPTH_32
#define BIT_DEPTH         (32)      /* Bit depth at 32 */
#endif

#if !(DEMO_PDM_MIC || DEMO_PDM_RECORD_PLAYBACK || DEMO_PCM_MIC || DEMO_PCM_RECORD_PLAYBACK || DEMO_MEM_TO_MEM)
#error "Only one audio demo should be selected!!!"
#endif

#if (DEMO_PDM_MIC + DEMO_PDM_RECORD_PLAYBACK + DEMO_PCM_MIC + DEMO_PCM_RECORD_PLAYBACK + DEMO_MEM_TO_MEM > 1)
#error "One audio demo should be selected!!!"
#endif

#if DEMO_PDM_RECORD_PLAYBACK || DEMO_PCM_RECORD_PLAYBACK
#include "ad_nvms.h"
#endif

#if DEMO_PDM_MIC || DEMO_PDM_RECORD_PLAYBACK
# if SR1_16_SR2_8
#define PDM_FREQ          (2000000)       /* Frequency of PDM mic at 2MHz */
# elif SR1_48_SR2_32
#define PDM_FREQ          (4000000)       /* Frequency of PDM mic at 2MHz */
# endif
#endif

#if DEMO_PDM_RECORD_PLAYBACK || DEMO_PCM_RECORD_PLAYBACK || DEMO_MEM_TO_MEM
#define NOTIF_TASK_TEST_DONE          (1 << 1)
#define DEMO_CHANNEL_DATA_BUF_BASIC_SIZE (1024)
#define BUFSIZE_FACTOR                       48
#define DEMO_CHANNEL_DATA_BUF_TOTAL_SIZE (DEMO_CHANNEL_DATA_BUF_BASIC_SIZE * BUFSIZE_FACTOR)
#define DEMO_CHANNEL_DATA_BUF_CB_SIZE     DEMO_CHANNEL_DATA_BUF_BASIC_SIZE
#endif

#define PRINTF_RECORDED_CHANNELS(ch) (ch == HW_PDM_CHANNEL_R ? "R" : \
                                       ch == HW_PDM_CHANNEL_L ? "L" : \
                                       ch == HW_PDM_CHANNEL_LR ? "L and R" : "none")

typedef enum {
        INPUT_DEVICE,
        OUTPUT_DEVICE
} device_direction_t;

typedef struct {
        void *audio_path;
        OS_TASK audio_task;
#if DEMO_PDM_RECORD_PLAYBACK || DEMO_PCM_RECORD_PLAYBACK || DEMO_MEM_TO_MEM
        uint32_t available_to_read;
# if DEMO_MEM_TO_MEM
        uint32_t available_to_read_2;
# endif
#endif
        sys_audio_device_t dev_in;
        sys_audio_device_t dev_out;
} context_demo_apu_t;

extern context_demo_apu_t context_demo_apu;

void printf_settings(sys_audio_device_t *dev, device_direction_t dir);
void audio_task(void *pvParameters);

#endif /* INCLUDES_AUDIO_TASK_H_ */
