/*
 * Copyright (c) 2013 - 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017, 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/


/*******************************************************************************
 * Prototypes
 ******************************************************************************/

typedef enum _hw_failure_mode
{
    HW_FailureModeStart   = 0,

    HW_FailureModeLedBlinky,
    HW_FailureModeCpuWfi,
    HW_FailureModeSystemReset,
    HW_FailureModePeriphRegAccess,
    HW_FailureModeFlashMemset,
    HW_FailureModeMpuRegionAccess,

    HW_FailureModeEnd = HW_FailureModeMpuRegionAccess
} hw_failure_mode_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/
volatile uint32_t g_systickCounter;

static hw_failure_mode_t s_targetFailureMode;

/*******************************************************************************
 * Code
 ******************************************************************************/
void SysTick_Handler(void)
{
    if (g_systickCounter != 0U)
    {
        g_systickCounter--;
    }
}

void SysTick_DelayTicks(uint32_t n)
{
    g_systickCounter = n;
    while (g_systickCounter != 0U)
    {
    }
}

#if defined(MIMXRT1176_cm7_SERIES)
uint32_t HWF_GetLastMode(void)
{
    return ((IOMUXC_SNVS_GPR->GPR32 & (~IOMUXC_SNVS_GPR_GPR32_LOCK_MASK))>> 1);
}

void HWF_SetLastMode(uint32_t val)
{
    IOMUXC_SNVS_GPR->GPR32 = (uint32_t)(((uint16_t)val) << 1);
}
#else
uint32_t HWF_GetLastMode(void)
{
    return IOMUXC_SNVS_GPR->GPR0;
}

void HWF_SetLastMode(uint32_t val)
{
    IOMUXC_SNVS_GPR->GPR0 = val;
}
#endif

void HWF_BlinkyLed(void)
{
    /* Set systick reload value to generate 1ms interrupt */
    if (SysTick_Config(SystemCoreClock / 1000U))
    {
        while (1)
        {
        }
    }
    uint8_t g_pinSetCnt = 0;
    while (1)
    {
        /* Delay 1000 ms */
        SysTick_DelayTicks(1000U);
        PRINTF("Toggle LED once.\r\n");
        if (g_pinSetCnt % 2)
        {
            GPIO_PinWrite(BOARD_USER_LED_GPIO, BOARD_USER_LED_GPIO_PIN, 0U);
        }
        else
        {
            GPIO_PinWrite(BOARD_USER_LED_GPIO, BOARD_USER_LED_GPIO_PIN, 1U);
        }
        g_pinSetCnt++;
        if (g_pinSetCnt > 8)
        {
            SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
            break;
        }
    }
}

void HWF_WfiCpu(void)
{
    HWF_SetLastMode((uint32_t)s_targetFailureMode);

    __asm volatile("dsb");
    /* wait for interrupt: the next interrupt will wake us up */
    __asm volatile("wfi");
    __asm volatile("isb");
}

void HWF_ResetSystem(void)
{
    HWF_SetLastMode((uint32_t)s_targetFailureMode);

    NVIC_SystemReset();
}

void HWF_AccessPeriphReg(void)
{
#if defined(MIMXRT1176_cm7_SERIES)
    CLOCK_PowerOffRootClock(kCLOCK_Root_Can1);
#else
    CLOCK_DisableClock(kCLOCK_Can1S);
#endif
    CLOCK_DisableClock(kCLOCK_Can1);

    hw_failure_mode_t mode = (hw_failure_mode_t)(HWF_GetLastMode());
    if (mode == HW_FailureModePeriphRegAccess)
    {
        uint32_t can1rxmgmask = CAN1->RXMGMASK;
        if (can1rxmgmask)
        {
            assert(false);
        }
    }
    HWF_SetLastMode((uint32_t)s_targetFailureMode);
  
    PRINTF("CAN1->MCR = %x\r\n", CAN1->MCR);
    PRINTF("CAN1->CTRL1 = %x\r\n", CAN1->CTRL1);
    PRINTF("CAN1->TIMER = %x\r\n", CAN1->TIMER);
    
    PRINTF("CAN1->RXMGMASK = %x\r\n", CAN1->RXMGMASK);
    PRINTF("CAN1->RX14MASK = %x\r\n", CAN1->RX14MASK);
    PRINTF("CAN1->RX15MASK = %x\r\n", CAN1->RX15MASK);
  
    //PRINTF("%x\r\n", CAN1->ECR);
    //PRINTF("%x\r\n", CAN1->ESR1);
    //PRINTF("%x\r\n", CAN1->IMASK2);
    //PRINTF("%x\r\n", CAN1->IMASK1);
    //PRINTF("%x\r\n", CAN1->IFLAG2);

    /*
    uint32_t *ptr = (uint32_t *)CAN1;
    for (uint32_t i = 4; i < 30; i++)
    {
        PRINTF("%x\r\n", *(ptr+i));
    }
   */
}

void HWF_MemsetFlash(void)
{
    HWF_SetLastMode((uint32_t)s_targetFailureMode);

#if defined(MIMXRT1176_cm7_SERIES)
    memset((void *)(FlexSPI1_AMBA_BASE + 128 *1024), 0x5A, 64 * 1024);
#else
    memset((void *)(FlexSPI_AMBA_BASE + 128 *1024), 0x5A, 64 * 1024);
#endif
}

void HWF_AccessMpuRegion(void)
{
    HWF_SetLastMode((uint32_t)s_targetFailureMode);

    memset((void *)0x20200000, 0x5A, 0x10);
}

void APP_FailureModeSelection(hw_failure_mode_t targetFailureMode)
{
    switch (targetFailureMode)
    {
        case HW_FailureModeLedBlinky:
            HWF_BlinkyLed();
            break;
        case HW_FailureModeCpuWfi:
            HWF_WfiCpu();
            break;
        case HW_FailureModeSystemReset:
            HWF_ResetSystem();
            break;
        case HW_FailureModePeriphRegAccess:
            HWF_AccessPeriphReg();
            break;
        case HW_FailureModeFlashMemset:
            HWF_MemsetFlash();
            break;
        case HW_FailureModeMpuRegionAccess:
            HWF_AccessMpuRegion();
            break;
        default:
            break;
    }
}

/*!
 * @brief Main function
 */
void hwf_main(void)
{
    BOARD_ConfigMPU();
    /* Enable I cache and D cache */
    //SCB_DisableDCache();
    //SCB_DisableICache();

    s_targetFailureMode = (hw_failure_mode_t)HWF_GetLastMode();
    APP_FailureModeSelection(s_targetFailureMode);

    /* Init board hardware. */
    BOARD_InitBootPins();
#if defined(MIMXRT1176_cm7_SERIES)
    BOARD_BootClockRUN();
#else
    BOARD_InitBootClocks();
#endif
    BOARD_InitDebugConsole();

    /* Just enable the trace clock, leave coresight initialization to IDE debugger */
    SystemCoreClockUpdate();
#if defined(MIMXRT1176_cm7_SERIES)
    CLOCK_EnableClock(kCLOCK_Cstrace);
#else
    CLOCK_EnableClock(kCLOCK_Trace);
#endif

    PRINTF("\r\n########## RT HW Failure Demo ###########\n\r\n");
    PRINTF("Select the desired mode \n\r\n");
    PRINTF("Press %c for enter case: LED blinky mode\r\n",                                  (uint8_t)'A' + (uint8_t)HW_FailureModeLedBlinky);
    PRINTF("Press %c for enter case: CPU WFI mode\r\n",                                     (uint8_t)'A' + (uint8_t)HW_FailureModeCpuWfi);
    PRINTF("Press %c for enter case: System soft reset mode\r\n",                           (uint8_t)'A' + (uint8_t)HW_FailureModeSystemReset);
    PRINTF("Press %c for enter case: Peripheral register access without clocking mode\r\n", (uint8_t)'A' + (uint8_t)HW_FailureModePeriphRegAccess);
    PRINTF("Press %c for enter case: Flash access via memset mode\r\n",                     (uint8_t)'A' + (uint8_t)HW_FailureModeFlashMemset);
    PRINTF("Press %c for enter case: MPU region access without permission mode\r\n",        (uint8_t)'A' + (uint8_t)HW_FailureModeMpuRegionAccess);
    PRINTF("\r\nWaiting for HW failure case select...\r\n\r\n");

    while (1)
    {
        char ch = GETCHAR();
        if ((ch >= 'a') && (ch <= 'z'))
        {
            ch -= 'a' - 'A';
        }
        PRINTF("Entering case %c\r\n", ch);
        s_targetFailureMode = (hw_failure_mode_t)(ch - 'A');

        if (s_targetFailureMode <= HW_FailureModeEnd)
        {
            APP_FailureModeSelection(s_targetFailureMode);
        }

        PRINTF("\r\nNext loop\r\n");
    }
}
