/**
 ****************************************************************************************
 *
 * @file hw_pcm.c
 *
 * @brief Implementation of the PCM interface Low Level Driver.
 *
 * Copyright (C) 2019-2022 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */
#if dg_configUSE_HW_PCM

#include "hw_pcm.h"
#include <hw_clk.h>
/*
 * Implementation of the binary Greatest Common Divisor (GCD) algorithm
 *
 * param[in] numerator is the numerator of the requested fraction
 * param[in] denominator is the denominator of the requested fraction
 *
 * return    the gcd
 *
 */
static uint32_t calculate_fraction_gcd(uint32_t numerator, uint32_t denominator)
{
        int pof2, tmp;

        if (!numerator || !denominator) {
                return (numerator | denominator);
        }

        // pof2 is the greatest power of 2 deviding both numbers .
        // We will use pof2 to multiply the returning number .
        pof2 = 0;

        while (!(numerator & 1) && !(denominator & 1)) {
                // gcd(even1, even1) = pof2 * gcd(even1/pof2, even2/pof2)
                numerator >>= 1;
                denominator >>= 1;
                pof2++;
        }

        do {
                while (!(numerator & 1)) {
                        numerator >>= 1;
                }

                while (!(denominator & 1)) {
                        denominator >>= 1;
                }

                // At this point we know for sure that
                // numerator and denominator are odd
                if (numerator >= denominator) {
                        numerator = (numerator - denominator) >> 1;
                } else {
                        tmp = numerator;
                        numerator = (denominator - numerator) >> 1;
                        denominator = tmp;
                }
        } while (!((numerator == denominator) || (numerator == 0)));

        return (denominator << pof2);
}

static void assert_pcm_init_clk_args(HW_PCM_CLOCK clock, uint8_t sample_rate, uint8_t bit_depth,
        uint8_t chs)
{
        /* verify sample rate is supported. 96ksps is not supported by pcm divisors table */
        ASSERT_WARNING((sample_rate == 8)  || (sample_rate == 16) ||
                (sample_rate == 32) || (sample_rate == 48));

        /* The value of bit depth must not exceed the size of single pcm register
         * and an integer multiplier of bytes*/
        ASSERT_WARNING((bit_depth == 8) || (bit_depth == 16) || (bit_depth == 24) || (bit_depth == 32));

        /* verify channel number is supported */
        ASSERT_WARNING((chs > 0) && (chs <= 2));
}

void hw_pcm_init_clk_reg(HW_PCM_CLOCK clock, uint8_t sample_rate, uint8_t bit_num, uint8_t channel_num,
        HW_PCM_CLK_GENERATION div)
{
        uint16_t bit_clock;
        uint32_t divider = 0;
        uint32_t sys_clk_freq = 0;

        assert_pcm_init_clk_args(clock, sample_rate, bit_num, channel_num);

        /* PCM clk configuration */
        /* Set DIVN/DIV1 */
        if (clock == HW_PCM_CLK_DIV1) {
                /* DIV1 clock used */
                REG_SET_BIT(CRG_PER, PCM_DIV_REG, PCM_SRC_SEL);
                /* Get sys clk frequency in KHz */
                sys_clk_freq = hw_clk_get_sysclk_freq()/1000;
        } else {
                /* DIVN clock used */
                REG_CLR_BIT(CRG_PER, PCM_DIV_REG, PCM_SRC_SEL);
                sys_clk_freq = 32000;
        }

        bit_clock = sample_rate * bit_num * channel_num;

        if (div == HW_PCM_CLK_GEN_FRACTIONAL) {
                uint16_t fdiv_fractional = 0x0;
                uint16_t fdiv_numerator = 0x0;
                uint32_t gcd = 0;

                uint32_t denominator = bit_clock;

                /* Find the numerator */
                divider = sys_clk_freq / bit_clock;

                uint32_t numerator = sys_clk_freq - (bit_clock * divider);

                if (numerator != 0) {

                        gcd = calculate_fraction_gcd(numerator, denominator);

                        if (gcd != 0) {
                                /* reduce the fraction by dividing both numerator and denominator by GCF */
                                numerator /= gcd;
                                denominator /= gcd;
                        }
                        /* Check if numerator and denominator are valid */
                        ASSERT_WARNING(numerator < denominator && denominator < 16);

                        /* Calculate fdiv_fractional */
                        fdiv_fractional = 0x1;
                        fdiv_numerator = 0xFFFF >> (16 - (numerator - 1));
                        fdiv_fractional = (fdiv_fractional << (denominator - 1)) | fdiv_numerator;
                }
                REG_SETF(CRG_PER, PCM_FDIV_REG, PCM_FDIV, fdiv_fractional);
        } else {
                uint16_t actual_bit_depth = bit_num;

                while (actual_bit_depth <= 64) {

                        if (sys_clk_freq % bit_clock == 0) {
                                break;
                        }
                        actual_bit_depth++;
                        /* Recalculate bit clock with actual bit depth*/
                        bit_clock = sample_rate * actual_bit_depth * channel_num;
                }

                ASSERT_WARNING(actual_bit_depth <= 64);

                divider = sys_clk_freq / bit_clock;
                REG_SETF(CRG_PER, PCM_FDIV_REG, PCM_FDIV, 0);
        }

        REG_SETF(CRG_PER, PCM_DIV_REG, PCM_DIV, divider);
        REG_SET_BIT(CRG_PER, PCM_DIV_REG, CLK_PCM_EN);
}

static void hw_pcm_init_generic_pcm(hw_pcm_config_generic_pcm_t *config)
{
        /* Set channel delay in multiples of 8 bits */
        hw_pcm_set_channel_delay(config->channel_delay);

        /* Set the number of clock cycles per data bit */
        hw_pcm_set_clk_per_bit(HW_PCM_ONE_CYCLE_PER_BIT);

        /* Set polarity of PCM FSC */
        hw_pcm_set_fsc_polarity(config->fsc_polarity);

        /* Set polarity of PCM CLK */
        hw_pcm_set_clk_polarity(config->clock_polarity);

        hw_pcm_set_fsc_delay(config->fsc_delay);

        /* FSC length */
        hw_pcm_set_fsc_length(config->fsc_length);

        /* Set PCM edge */
        hw_pcm_set_fsc_edge(HW_PCM_FSC_EDGE_RISING);

        /* Set PCM FSC divider */
        hw_pcm_set_fsc_div(config->fsc_div);

        /* Generic PCM configuration done successfully */
}

static void hw_pcm_init_i2s(hw_pcm_config_i2s_mode_t *config)
{
        hw_pcm_set_channel_delay(0);

        /* Set PCM edge */
        hw_pcm_set_fsc_edge(HW_PCM_FSC_EDGE_RISING_AND_FALLING);

        /* Set the number of clock cycles per data bit */
        hw_pcm_set_clk_per_bit(HW_PCM_ONE_CYCLE_PER_BIT);

        /* Set polarity of PCM FSC */
        hw_pcm_set_fsc_polarity(config->fsc_polarity);

        /* Set polarity of PCM CLK */
        hw_pcm_set_clk_polarity(HW_PCM_CLK_POLARITY_INVERTED);

        hw_pcm_set_fsc_delay(HW_PCM_FSC_STARTS_1_CYCLE_BEFORE_MSB_BIT);

        /* FSC length */
        hw_pcm_set_fsc_length(config->fsc_length);

        /* Set PCM FSC divider */
        hw_pcm_set_fsc_div(config->fsc_div);
}

static void hw_pcm_set_init_tdm(hw_pcm_config_tdm_mode_t *config)
{
        hw_pcm_set_channel_delay(config->channel_delay);

        /* Set PCM edge */
        hw_pcm_set_fsc_edge(HW_PCM_FSC_EDGE_RISING_AND_FALLING);

        /* Set the number of clock cycles per data bit */
        hw_pcm_set_clk_per_bit(HW_PCM_ONE_CYCLE_PER_BIT);

        /* Set polarity of PCM FSC */
        hw_pcm_set_fsc_polarity(config->fsc_polarity);

        /* Set polarity of PCM CLK */
        hw_pcm_set_clk_polarity(HW_PCM_CLK_POLARITY_INVERTED);

        hw_pcm_set_fsc_delay(HW_PCM_FSC_STARTS_SYNCH_TO_MSB_BIT);

        /* FSC length */
        hw_pcm_set_fsc_length(config->fsc_length);

        /* Set PCM FSC divider */
        hw_pcm_set_fsc_div(config->fsc_div);
}

static void hw_pcm_set_init_iom(hw_pcm_config_iom_mode_t *config)
{
        hw_pcm_set_channel_delay(0);

        /* Set PCM edge */
        hw_pcm_set_fsc_edge(HW_PCM_FSC_EDGE_RISING);

        /* Set the number of clock cycles per data bit */
        hw_pcm_set_clk_per_bit(HW_PCM_TWO_CYCLE_PER_BIT);

        /* Set polarity of PCM FSC */
        hw_pcm_set_fsc_polarity(config->fsc_polarity);

        /* Set polarity of PCM CLK */
        hw_pcm_set_clk_polarity(HW_PCM_CLK_POLARITY_NORMAL);

        hw_pcm_set_fsc_delay(HW_PCM_FSC_STARTS_SYNCH_TO_MSB_BIT);

        /* FSC length */
        hw_pcm_set_fsc_length(0);

        /* For 2 clock cycles per bit fsc_div must be even */
        ASSERT_WARNING((config->fsc_div % 2) == 0);

        /* Set PCM Frame synchronization divider */
        hw_pcm_set_fsc_div(config->fsc_div);
}

void hw_pcm_init(hw_pcm_config_t *config)
{
        /* Disable PCM */
        hw_pcm_disable();

        /* Write zero value to output registers to force the unused channels to zero */
        hw_pcm_output_write(HW_PCM_OUTPUT_REG_1, 0);
        hw_pcm_output_write(HW_PCM_OUTPUT_REG_2, 0);

        hw_pcm_set_output_mode(config->gpio_output_mode);

        /* Set PCM in Master Mode */
        hw_pcm_set_mode(config->pcm_mode);

        /* When fsc_edge == HW_PCM_FSC_EDGE_RISING and two channels are used then
         * the bits_depth must be 32bits because there is no way to define when the
         * bits of the first channel end and where the bits of the second channel begin.
         */
        switch (config->config_mode) {
        case HW_PCM_CONFIG_GENERIC_PCM_MODE:
                hw_pcm_init_generic_pcm(&config->pcm_param);
                break;
        case HW_PCM_CONFIG_I2S_MODE:
                hw_pcm_init_i2s(&config->i2s_param);
                break;
        case HW_PCM_CONFIG_TDM_MODE:
                hw_pcm_set_init_tdm(&config->tdm_param);
                break;
        case HW_PCM_CONFIG_IOM_MODE:
                hw_pcm_set_init_iom(&config->iom_param);
                break;
        default:
                ASSERT_WARNING(0);
        }
}

#endif /* dg_configUSE_HW_PCM */
