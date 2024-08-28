/**
 ****************************************************************************************
 *
 * @file bsp_device_definitions_internal.h
 *
 * @brief Board Support Package. Device information attributes definitions.
 *
 * Copyright (C) 2019-2021 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef BSP_DEVICE_DEFINITIONS_INTERNAL_H_
#define BSP_DEVICE_DEFINITIONS_INTERNAL_H_

/*
 * All device's information attributes bit-fields are OR'ed in a 32-bit value according to the
 * following schema:
 *
 *  31     25 24    23 22               13     12       11              8 7               0
 *  +--------+--------+-------------------+------------+-----------------+-----------------+
 *  |        |        |                   |            |          Device Version           |
 *  |--------|--------|-------------------|------------------------------------------------|
 *  | Device | Device |  Device Variant   | 0: silicon | Device Revision | Device Step     |
 *  | Family | Chip ID|                   |            |   |r|r|r|r|     | |s|s|s|s|s|s|s|s|
 *  |  |.|6|6|    |2|3|x|x|x|x|x|x|x|x|x|x|            |   |D|C|B|A|     | |H|G|F|E|D|C|B|A|
 *  |  |.|9|8|    |5|0|y|y|y|y|y|y|y|y|y|y|------------------------------------------------|
 *  |  |.|z|z|    |2|8|9|8|7|6|5|4|3|2|1|0|  1: FPGA   |            FPGA version           |
 *  |        |    |2|0|                   |            |        (numerical 0-4095)         |
 *  +--------+--------+-------------------+------------+-----------------------------------+
 *
 * [31:25] Device Family
 *              0x000: invalid
 *              0x001: 68z
 *              0x002: 69z
 *              0x004: reserved
 *              0x008: reserved
 *              0x010: reserved
 *              0x020: reserved
 *              0x040: reserved
 *
 * [24:23] Device Chip ID:
 *              0x000: invalid
 *              0x001: 680
 *              0x001: 3080
 *              0x002: 2522
 *
 * [22:13] Device Variant
 *              0x000: invalid for silicon, used for FPGA only
 *              0x001: xy0
 *              0x002: xy1
 *              0x004: xy2
 *              0x008: xy3
 *              0x010: xy4
 *              0x020: xy5
 *              0x040: xy6
 *              0x080: xy7
 *              0x100: xy9
 *
 * [12] Determines whether the device is FPGA or silicon
 *              0x000: Silicon
 *              0x001: FPGA
 *
 *  [11:0] Device Version
 *      Silicon (Bit field [12] is 0):
 *              [7:0] Device Step (e.g. xB)
 *                      0x000: invalid
 *                      0x001: A
 *                      0x002: B
 *                      0x004: C
 *                      0x008: D
 *                      0x010: E
 *                      0x020: F
 *                      0x040: G
 *                      0x080: H
 *              [11:8] Device Revision (e.g. Ax)
 *                      0x000: invalid
 *                      0x001: A
 *                      0x002: B
 *                      0x004: C
 *                      0x008: D
 *      FPGA (Bit field [12] is 1):
 *              [11:0] FPGA image version:
 *                      0-4095: numerical version matching single FPGA image version
 *
 */

/*
 * Internal definitions
 */

/* Make bit field value */
#define _DEVICE_MK_BF_VAL(field, value)         (((1 << value) << _DEVICE_ ## field ## _POS) & _DEVICE_ ## field ## _MASK)
/* Make numerical value */
#define _DEVICE_MK_NUM_VAL(field, value)        (((value) << _DEVICE_ ## field ## _POS) & _DEVICE_ ## field ## _MASK)

/* Device family definitions */
#define _DEVICE_FAMILY_MASK             0xFE000000
#define _DEVICE_FAMILY_POS              25
#define _DEVICE_FAMILY_680              0
#define _DEVICE_FAMILY_690              1
#define _DEVICE_MK_FAMILY(x)            _DEVICE_MK_BF_VAL(FAMILY, _DEVICE_FAMILY_ ## x)

/* Device Chip ID definitions */
#define _DEVICE_CHIP_ID_MASK            0x01800000
#define _DEVICE_CHIP_ID_POS             23
#define _DEVICE_CHIP_ID_680             0
#define _DEVICE_CHIP_ID_3080            0
#define _DEVICE_CHIP_ID_2522            1
#define _DEVICE_MK_CHIP_ID(x)           _DEVICE_MK_BF_VAL(CHIP_ID, _DEVICE_CHIP_ID_ ## x)

/* Device variants definitions */
#define _DEVICE_VARIANT_MASK            0x007FE000
#define _DEVICE_VARIANT_POS             13
#define _DEVICE_VARIANT_XX0             0
#define _DEVICE_VARIANT_XX1             1
#define _DEVICE_VARIANT_XX2             2
#define _DEVICE_VARIANT_XX3             3
#define _DEVICE_VARIANT_XX4             4
#define _DEVICE_VARIANT_XX5             5
#define _DEVICE_VARIANT_XX6             6
#define _DEVICE_VARIANT_XX7             7
#define _DEVICE_VARIANT_XX8             8
#define _DEVICE_VARIANT_XX9             9
#define _DEVICE_MK_VARIANT(x)           _DEVICE_MK_BF_VAL(VARIANT, _DEVICE_VARIANT_XX ## x)

/* FPGA definitions */
#define _DEVICE_FPGA_MASK               0x00001000
#define _DEVICE_FPGA_POS                12

#define _DEVICE_FPGA_VER_MASK           0x00000FFF
#define _DEVICE_FPGA_VER_POS            0
#define _DEVICE_MK_FPGA_VER(n)          _DEVICE_MK_NUM_VAL(FPGA_VER, (n))

/* Device stepping major subrevisions */
#define _DEVICE_REV_MASK                0x00000F00
#define _DEVICE_REV_POS                 8
#define _DEVICE_REV_A                   0
#define _DEVICE_REV_B                   1
#define _DEVICE_REV_C                   2
#define _DEVICE_REV_D                   3
#define _DEVICE_MK_REV(x)               _DEVICE_MK_BF_VAL(REV, _DEVICE_REV_ ## x)

/* Device stepping minor subrevisions */
#define _DEVICE_STEP_MASK               0x000000FF
#define _DEVICE_STEP_POS                0
#define _DEVICE_STEP_A                  0
#define _DEVICE_STEP_B                  1
#define _DEVICE_STEP_C                  2
#define _DEVICE_STEP_D                  3
#define _DEVICE_STEP_E                  4
#define _DEVICE_STEP_F                  5
#define _DEVICE_STEP_G                  6
#define _DEVICE_STEP_H                  7
#define _DEVICE_MK_STEP(y)              _DEVICE_MK_BF_VAL(STEP, _DEVICE_STEP_ ## y)
#define _DEVICE_MK_VER(x, y)            (_DEVICE_MK_REV(x) | _DEVICE_MK_STEP(y))

#define _DEVICE_MASK                    (_DEVICE_FAMILY_MASK | _DEVICE_CHIP_ID_MASK |        \
                                         _DEVICE_VARIANT_MASK | _DEVICE_FPGA_MASK)

/* Public definitions */

/* DA1468X Device Variants */
#define DA14680                         (_DEVICE_MK_FAMILY(680) | _DEVICE_MK_VARIANT(0))
#define DA14681                         (_DEVICE_MK_FAMILY(680) | _DEVICE_MK_VARIANT(1))
#define DA14682                         (_DEVICE_MK_FAMILY(680) | _DEVICE_MK_VARIANT(2))
#define DA14683                         (_DEVICE_MK_FAMILY(680) | _DEVICE_MK_VARIANT(3))
/* FPGA Device */
#define DA1468X_FPGA                    (_DEVICE_MK_FAMILY(680) | _DEVICE_MK_CHIP_ID(680) |_DEVICE_FPGA_MASK)
/* Family Wildcard */
#define DA1468X                         (_DEVICE_MK_FAMILY(680))

/* DA1469X Device Variants */
#define DA14691                         (_DEVICE_MK_FAMILY(690) | _DEVICE_MK_VARIANT(1))
#define DA14693                         (_DEVICE_MK_FAMILY(690) | _DEVICE_MK_VARIANT(3))
#define DA14695                         (_DEVICE_MK_FAMILY(690) | _DEVICE_MK_VARIANT(5))
#define DA14697                         (_DEVICE_MK_FAMILY(690) | _DEVICE_MK_VARIANT(7))
#define DA14699                         (_DEVICE_MK_FAMILY(690) | _DEVICE_MK_VARIANT(9))
/* FPGA Device */
#define D2522                           (_DEVICE_MK_FAMILY(690) | _DEVICE_MK_CHIP_ID(D2522) | _DEVICE_FPGA_MASK)
/* Family Wildcard */
#define DA1469X                         (_DEVICE_MK_FAMILY(690))

/* Device Chip ID */
#define DEVICE_CHIP_ID_680              (_DEVICE_MK_CHIP_ID(680))
#define DEVICE_CHIP_ID_3080             (_DEVICE_MK_CHIP_ID(3080))
#define DEVICE_CHIP_ID_2522             (_DEVICE_MK_CHIP_ID(2522))

/* Device Revision */
#define DEVICE_REV_A                    (_DEVICE_MK_REV(A))
#define DEVICE_REV_B                    (_DEVICE_MK_REV(B))

/* Device Step */
#define DEVICE_STEP_A                   (_DEVICE_MK_STEP(A))
#define DEVICE_STEP_B                   (_DEVICE_MK_STEP(B))
#define DEVICE_STEP_C                   (_DEVICE_MK_STEP(C))
#define DEVICE_STEP_D                   (_DEVICE_MK_STEP(D))
#define DEVICE_STEP_E                   (_DEVICE_MK_STEP(E))
#define DEVICE_STEP_F                   (_DEVICE_MK_STEP(F))
#define DEVICE_STEP_G                   (_DEVICE_MK_STEP(G))
#define DEVICE_STEP_H                   (_DEVICE_MK_STEP(H))

/* Device Version */
#define DEVICE_VER_AA                   (_DEVICE_MK_VER(A, A))
#define DEVICE_VER_AB                   (_DEVICE_MK_VER(A, B))
#define DEVICE_VER_AE                   (_DEVICE_MK_VER(A, E))
#define DEVICE_VER_BB                   (_DEVICE_MK_VER(B, B))

/*
 * A generic FPGA check, available for any device family.
 */
#define DEVICE_FPGA                     ((dg_configDEVICE & _DEVICE_FPGA_MASK) == _DEVICE_FPGA_MASK)

/*
 * Macros checking against specific device characteristics.
 * Examples:
 * #if (DEVICE_FAMILY == DA1468X)
 * #if (DEVICE_CHIP_ID == DEVICE_CHIP_ID_3080)
 * #if ((DEVICE_VARIANT == DA14695) || (DEVICE_VARIANT == DA14699))
 * #if (DEVICE_REVISION == DEVICE_REV_B)
 * #if ((DEVICE_VERSION == DEVICE_VER_AB) || (DEVICE_VERSION == DEVICE_VER_AE))
 */
#define DEVICE_FAMILY                   (dg_configDEVICE & _DEVICE_FAMILY_MASK)
#define DEVICE_CHIP_ID                  (dg_configDEVICE & _DEVICE_CHIP_ID_MASK)
#define DEVICE_VARIANT                  (dg_configDEVICE & (_DEVICE_FAMILY_MASK | _DEVICE_VARIANT_MASK))
#define DEVICE_REVISION                 (dg_configDEVICE & _DEVICE_REV_MASK)
#define DEVICE_STEP                     (dg_configDEVICE & _DEVICE_STEP_MASK)
#define DEVICE_VERSION                  (dg_configDEVICE & (_DEVICE_REV_MASK | _DEVICE_STEP_MASK))

/*
 * Device information attributes masks
 */
#define DEVICE_FAMILY_MASK              (_DEVICE_FAMILY_MASK)
#define DEVICE_CHIP_ID_MASK             (_DEVICE_CHIP_ID_MASK)
#define DEVICE_VARIANT_MASK             (_DEVICE_VARIANT_MASK)
#define DEVICE_REVISION_MASK            (_DEVICE_REV_MASK)
#define DEVICE_STEP_MASK                (_DEVICE_STEP_MASK)

/*
 * Use the next macros to get the minimum acceptable value of specific device information attribute
 */
#define DEVICE_INFO_ATTRIBUTE_MIN(mask) (1 << __CLZ(__RBIT(mask)))
#define DEVICE_FAMILY_MIN               (1 << _DEVICE_FAMILY_POS)
#define DEVICE_CHIP_ID_MIN              (1 << _DEVICE_CHIP_ID_POS)
#define DEVICE_VARIANT_MIN              (1 << _DEVICE_VARIANT_POS)
#define DEVICE_REVISION_MIN             (1 << _DEVICE_REV_POS)
#define DEVICE_STEP_MIN                 (1 << _DEVICE_STEP_POS)

/*
 * Use the next macros to get the maximum acceptable value of specific device information attribute
 */
#define DEVICE_INFO_ATTRIBUTE_MAX(mask) (1 << (31 - __CLZ(mask)))
#define DEVICE_FAMILY_MAX               DEVICE_INFO_ATTRIBUTE_MAX(_DEVICE_FAMILY_MASK)
#define DEVICE_CHIP_ID_MAX              DEVICE_INFO_ATTRIBUTE_MAX(_DEVICE_CHIP_ID_MASK)
#define DEVICE_VARIANT_MAX              DEVICE_INFO_ATTRIBUTE_MAX(_DEVICE_VARIANT_MASK)
#define DEVICE_REVISION_MAX             DEVICE_INFO_ATTRIBUTE_MAX(_DEVICE_REV_MASK)
#define DEVICE_STEP_MAX                 DEVICE_INFO_ATTRIBUTE_MAX(_DEVICE_STEP_MASK)

/*
 * Use the next macros to convert (mask and shift) a specific device information attribute
 * extracted from the corresponding registers to bit-field.
 */

#define MAKE_DEVICE_FAMILY_BITFIELD(family)     _DEVICE_MK_BF_VAL(FAMILY, family)
#define MAKE_DEVICE_CHIP_ID_BITFIELD(id)        _DEVICE_MK_BF_VAL(CHIP_ID, id)
#define MAKE_DEVICE_VARIANT_BITFIELD(variant)   _DEVICE_MK_BF_VAL(VARIANT, variant)
#define MAKE_DEVICE_REVISION_BITFIELD(rev)      _DEVICE_MK_BF_VAL(REV, rev)
#define MAKE_DEVICE_STEP_BITFIELD(step)         _DEVICE_MK_BF_VAL(STEP, step)

#endif /* BSP_DEVICE_DEFINITIONS_INTERNAL_H_ */
