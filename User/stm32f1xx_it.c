/**
 ******************************************************************************
 * @file    UART/UART_TwoBoards_ComIT/Src/stm32f1xx_it.c
 * @author  MCD Application Team
 * @brief   Main Interrupt Service Routines.
 *          This file provides template for all exceptions handler and
 *          peripherals interrupt service routine.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2016 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_it.h"
#include "timer.h"
/** @addtogroup STM32F1xx_HAL_Examples
 * @{
 */

/** @addtogroup UART_TwoBoards_ComIT
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* UART handler declared in "main.c" file */

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
 * @brief   This function handles NMI exception.
 * @param  None
 * @retval None
 */
void NMI_Handler(void)
{
}

/**
 * @brief  This function handles Hard Fault exception.
 * @param  None
 * @retval None
 */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
 * @brief  This function handles Memory Manage exception.
 * @param  None
 * @retval None
 */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
 * @brief  This function handles Bus Fault exception.
 * @param  None
 * @retval None
 */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
 * @brief  This function handles Usage Fault exception.
 * @param  None
 * @retval None
 */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
 * @brief  This function handles SVCall exception.
 * @param  None
 * @retval None
 */
//void SVC_Handler(void)
//{
//}

/**
 * @brief  This function handles Debug Monitor exception.
 * @param  None
 * @retval None
 */
void DebugMon_Handler(void)
{
}

/**
 * @brief  This function handles PendSVC exception.
 * @param  None
 * @retval None
 */
//void PendSV_Handler(void)
//{
//}

/**
 * @brief  This function handles SysTick Handler.
 * @param  None
 * @retval None
 */
#if SYS_SUPPORT_OS

/* 添加公共头文件 ( rtos需要用到) */


/**
 * @brief       systick中断服务函数,使用OS时用到
 * @param       ticks: 延时的节拍数
 * @retval      无
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
#else
void SysTick_Handler(void)
{
    HAL_IncTick();
}
#endif
/******************************************************************************/
/*                 STM32F1xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f1xx.s).                                               */
/******************************************************************************/
/**
 * @brief  This function handles UART interrupt request.
 * @param  None
 * @retval None
 * @Note   This function is redefined in "main.h" and related to DMA
 *         used for USART data transmission
 */
void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&uart1.uart);

  if (__HAL_UART_GET_FLAG(&uart1.uart, UART_FLAG_IDLE))
  {
    __HAL_UART_CLEAR_IDLEFLAG(&uart1.uart);
    uart1.RxCounter += (U1_RX_MAX - __HAL_DMA_GET_COUNTER(&uart1.dmarx));
    HAL_UART_AbortReceive_IT(&uart1.uart);
  }
}
//void USART2_IRQHandler(void)
//{
//  HAL_UART_IRQHandler(&uart2.uart);

//  if (__HAL_UART_GET_FLAG(&uart2.uart, UART_FLAG_IDLE))
//  {
//    __HAL_UART_CLEAR_IDLEFLAG(&uart2.uart);
//    uart2.RxCounter += (U2_RX_MAX - __HAL_DMA_GET_COUNTER(&uart2.dmarx));
//    HAL_UART_AbortReceive_IT(&uart2.uart);
//  }
//}
//void USART3_IRQHandler(void)
//{
//  HAL_UART_IRQHandler(&uart3.uart);

//  if (__HAL_UART_GET_FLAG(&uart3.uart, UART_FLAG_IDLE))
//  {
//    __HAL_UART_CLEAR_IDLEFLAG(&uart3.uart);
//    uart3.RxCounter += (U3_RX_MAX - __HAL_DMA_GET_COUNTER(&uart3.dmarx));
//    HAL_UART_AbortReceive_IT(&uart3.uart);
//  }
//}

//void TIM3_IRQHandler(void)
//{
//	HAL_TIM_IRQHandler(&tim3);   //¶¨Ê±Æ÷3ÖÐ¶Ï´¦Àíº¯Êý
//}

/**
 * @brief  This function handles PPP interrupt request.
 * @param  None
 * @retval None
 */
/*void PPP_IRQHandler(void)
{
}*/

/**
 * @}
 */

/**
 * @}
 */
