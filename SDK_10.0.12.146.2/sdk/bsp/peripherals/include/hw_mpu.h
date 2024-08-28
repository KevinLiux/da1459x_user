/**
 * \addtogroup PLA_DRI_MEMORY
 * \{
 * \addtogroup HW_MPU Memory Protection Unit Low Level Driver
 * \{
 * \brief MPU Driver
 */

/**
 ****************************************************************************************
 *
 * @file hw_mpu.h
 *
 * @brief Definition of API for the Memory Protection Unit (MPU) Low Level Driver.
 *
 * The MPU is an optional ARM CM33 feature supported in DA14yyx SoC families that enables
 * protecting loosely defined regions of system RAM memory through enforcing privilege and
 * access rules per region. All MPU LLD terminology in based on the ARM CM33 nomenclature.
 *
 * Copyright (C) 2017-2021 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef HW_MPU_H_
#define HW_MPU_H_


#if dg_configUSE_HW_MPU

#include <sdk_defs.h>

/**
 * \brief Region Definitions
 *
 * The MPU divides the memory map into a number of eight regions. Each region has a defined
 * memory type, and memory attributes that determine the behaviour of accesses to the region.
 * A background (or default) region numbered as -1 exists with the same access attributes as
 * the generic memory map, but is accessible from privileged software only.
 */
typedef enum {
        HW_MPU_REGION_0 = 0,   /**< MPU region 0 */
        HW_MPU_REGION_1 = 1,   /**< MPU region 1 */
        HW_MPU_REGION_2 = 2,   /**< MPU region 2 */
        HW_MPU_REGION_3 = 3,   /**< MPU region 3 */
        HW_MPU_REGION_4 = 4,   /**< MPU region 4 */
        HW_MPU_REGION_5 = 5,   /**< MPU region 5 */
        HW_MPU_REGION_6 = 6,   /**< MPU region 6 */
        HW_MPU_REGION_7 = 7,   /**< MPU region 7 */
} HW_MPU_REGION_NUM;

/**
 * \brief Executable Region
 *
 * Attribute regarding the code execution from a particular region. The XN (eXecute Never) flag
 * must be zero and there must be read access for the privilege level in order to execute code
 * from the region, otherwise a memory manage (MemManage) fault is generated.
 */
typedef enum {
        HW_MPU_XN_FALSE = 0x00,        /**< Executable region */
        HW_MPU_XN_TRUE = 0x1,          /**< Execute never region */
} HW_MPU_XN;

/**
 * \brief Privileges to Read/Write
 *
 * Attribute regarding the access permission (AP) of a particular region with respect to privilege
 * level and read/write capabilities. Depending on the privilege configuration an application can
 * access or not CPU features such as memory, I/O, enable/disable interrupts, setup the NVIC, etc.
 * By system design it can be imperative to restrict an application by defining accordingly the
 * MPU settings for the corresponding region.
 */
typedef enum {
        HW_MPU_AP_PRIVRW = 0x0,         /**< Read/write by privileged code only */
        HW_MPU_AP_RW = 0x2,             /**< Read/write by any privilege level */
        HW_MPU_AP_PRIVRO = 0x4,         /**< Read-only by privileged code only */
        HW_MPU_AP_RO = 0x6,             /**< Read-only by any privilege level */
} HW_MPU_AP;

/**
 * \brief Memory Type
 *
 * Attribute regarding the memory type of a particular region. According to ARM CM33 nomenclature
 * two memory types are defined: device memory pertains to a memory-mapped region for a peripheral,
 * while normal memory is instead relevant to CPU use.
 */
typedef enum {
        HW_MPU_ATTR_DEVICE = 0x00,              /**< Device Memory */
        HW_MPU_ATTR_NORMAL = 0x44,              /**< Normal memory */
} HW_MPU_ATTR;

/**
 * \brief Memory Region Configuration
 */
typedef struct {
        uint32_t start_addr;            /**< MPU region start address. Must be multiple of 32 bytes. */
        uint32_t end_addr;              /**< MPU region end address. Must be multiple of 32 bytes. */
        HW_MPU_AP access_permissions;   /**< MPU region access permissions */
        HW_MPU_XN execute_never;        /**< Defines whether code can be executed from this region */
        HW_MPU_ATTR attributes;         /**< MPU region's memory attributes */
} mpu_region_config;

/**
 * \brief Initializes the MPU by disabling its operation during faults, defining the background region
 *              privilege access and finally by enabling the actual HW block.
 *
 * \param [in] privdefena Controls (enable/disable) privileged access to the background region.
 *
 *              When disabled, any access to the background region will cause a memory manage fault.
 *              When enabled, privileged accesses to the background region are allowed.
 *
 *              In handler mode, execution is always privileged. In thread mode
 *              privilege level can be set using the 'nPRIV' field of the control register.
 *              For manipulating nPRIV, check __set_CONTROL() and __get_CONTROL() CMSIS API calls.
 *              Hard fault and NMI handlers always operate with MPU disabled, accessing the
 *              default memory map as normal. The same can be true when FAULTMASK is set to 1,
 *              effectively masking Hard Fault exceptions by raising the current priority level to -1.
 *              FAULTMASK can only be set in privileged mode except from within NMI and HardFault
 *              Handlers (in which cases lockup state will be entered).
 *
 */
__STATIC_FORCEINLINE void hw_mpu_enable(bool privdefena)
{
        /* Disables the operation of MPU during hard fault, NMI, and FAULTMASK handlers. */
        REG_SETF(MPU, CTRL, HFNMIENA, 0);
        /* Controls (enable/disable) privileged access to the background region. */
        REG_SETF(MPU, CTRL, PRIVDEFENA, privdefena);
        /* Enables the MPU block */
        REG_SETF(MPU, CTRL, ENABLE, 1);

        __DSB();
        __ISB();
}

/**
 * \brief Disables the MPU
 *
 */
__STATIC_FORCEINLINE void hw_mpu_disable(void)
{
        __DMB();
        REG_SETF(MPU, CTRL, ENABLE, 0);
        __ISB();
}

/**
 * \brief Configures an MPU region.
 *
 * Region's start and end addresses will be aligned to 32 byte boundary. The start address
 * is logically ANDed with 0xFFFFFFE0 whereas the end address is logically ORed with 0x1F.
 *
 * The following accesses will generate a hard fault:
 * - An access to an address that matches in more than one region.
 * - An access that does not match all the access conditions for that region.
 * - An access to the background region, depending on the privilege mode and the value of
 *      the 'privdefena' parameter when MPU is enabled.
 *
 * \param [in] region_num Region number
 * \param [in] cfg Region configuration. When cfg is NULL the particular region is disabled.
*/
__STATIC_FORCEINLINE void hw_mpu_config_region(HW_MPU_REGION_NUM region_num, mpu_region_config *cfg)
{
        if (!cfg) {
                MPU->RNR = region_num;
                MPU->RBAR = 0x0;
                MPU->RLAR = 0x0;
        } else {
                ASSERT_WARNING ((cfg->start_addr & 0x1F) == 0);
                ASSERT_WARNING ((cfg->end_addr & 0x1F) == 0x1F);

                if (region_num < 4) {
                        MPU->MAIR0 |= cfg->attributes << (4 * region_num);
                } else {
                        MPU->MAIR1 |= cfg->attributes << (4 * (region_num - 4));
                }

                MPU->RNR = region_num;
                MPU->RBAR = (cfg->start_addr & 0xFFFFFFE0) | cfg->access_permissions | cfg->execute_never;
                MPU->RLAR = (cfg->end_addr | 0x1F) | (region_num << 1) | 0x1;
        }
}

#endif /* dg_configUSE_HW_MPU */


#endif /* HW_MPU_H_ */

/**
 * \}
 * \}
 */
