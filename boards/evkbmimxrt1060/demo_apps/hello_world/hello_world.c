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
#define EXAMPLE_LED_GPIO     BOARD_USER_LED_GPIO
#define EXAMPLE_LED_GPIO_PIN BOARD_USER_LED_PIN

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

typedef enum _hw_failure_mode
{
    HW_FailureModeLed = 0,
    HW_FailureModeWfi,
    HW_FailureModeAccessPeriph,

    HW_FailureModeEnd = HW_FailureModeAccessPeriph
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
            GPIO_PinWrite(EXAMPLE_LED_GPIO, EXAMPLE_LED_GPIO_PIN, 0U);
        }
        else
        {
            GPIO_PinWrite(EXAMPLE_LED_GPIO, EXAMPLE_LED_GPIO_PIN, 1U);
        }
        g_pinSetCnt++;
        if (g_pinSetCnt > 8)
        {
            SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
            break;
        }
    }
}

void HWF_EnterWFI(void)
{
    __asm volatile("dsb");
    /* wait for interrupt: the next interrupt will wake us up */
    __asm volatile("wfi");
    __asm volatile("isb");
}

void HWF_AccessPeriph(void)
{
    //PRINTF("%x\r\n", CAN1->MCR);
    //PRINTF("%x\r\n", CAN1->CTRL1);
    //PRINTF("%x\r\n", CAN1->TIMER);
    
    PRINTF("%x\r\n", CAN1->RXMGMASK);
    PRINTF("%x\r\n", CAN1->RX14MASK);
    PRINTF("%x\r\n", CAN1->RX15MASK);
  
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

void APP_FailureModeSelection(hw_failure_mode_t targetFailureMode)
{
    switch (targetFailureMode)
    {
        case HW_FailureModeLed:
            HWF_BlinkyLed();
            break;
        case HW_FailureModeWfi:
            HWF_EnterWFI();
            break;
        case HW_FailureModeAccessPeriph:
            HWF_AccessPeriph();
            break;
        default:
            assert(false);
            break;
    }
}

/*!
 * @brief Main function
 */
int main(void)
{
    char ch;

    /* Init board hardware. */
    BOARD_ConfigMPU();
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

    /* Just enable the trace clock, leave coresight initialization to IDE debugger */
    SystemCoreClockUpdate();
    CLOCK_EnableClock(kCLOCK_Trace);

    PRINTF("\r\n########## HW failure Demo ###########\n\r\n");
    PRINTF("\r\nSelect the desired operation \n\r\n");
    PRINTF("Press %c for enter case: LED blinky mode\r\n", (uint8_t)'A' + (uint8_t)HW_FailureModeLed);
    PRINTF("Press %c for enter case: CPU WFI mode\r\n", (uint8_t)'A' + (uint8_t)HW_FailureModeWfi);
    PRINTF("Press %c for enter case: Peripheral access before clocking mode\r\n", (uint8_t)'A' + (uint8_t)HW_FailureModeAccessPeriph);
    PRINTF("\r\nWaiting for HW failure case select...\r\n\r\n");

    while (1)
    {
        ch = GETCHAR();
        if ((ch >= 'a') && (ch <= 'z'))
        {
            ch -= 'a' - 'A';
        }
        s_targetFailureMode = (hw_failure_mode_t)(ch - 'A');

        if (s_targetFailureMode <= HW_FailureModeEnd)
        {
            APP_FailureModeSelection(s_targetFailureMode);
        }

        PRINTF("\r\nNext loop\r\n");
    }
}
