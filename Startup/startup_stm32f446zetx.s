  .syntax unified
  .cpu cortex-m4
  .fpu softvfp
  .thumb

  .global g_pfnVectors
  .global Default_Handler

  .word _sidata
  .word _sdata
  .word _edata
  .word _sbss
  .word _ebss

  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr   sp, =_estack

  /* Copy .data from FLASH to RAM */
  movs  r1, #0
  b     LoopCopyDataInit

CopyDataInit:
  ldr   r3, =_sidata
  ldr   r3, [r3, r1]
  str   r3, [r0, r1]
  adds  r1, r1, #4

LoopCopyDataInit:
  ldr   r0, =_sdata
  ldr   r3, =_edata
  adds  r2, r0, r1
  cmp   r2, r3
  bcc   CopyDataInit

  /* Zero .bss */
  ldr   r2, =_sbss
  b     LoopFillZerobss

FillZerobss:
  movs  r3, #0
  str   r3, [r2], #4

LoopFillZerobss:
  ldr   r3, =_ebss
  cmp   r2, r3
  bcc   FillZerobss

  /* Call SystemInit then main */
  bl    SystemInit
  bl    main
  bx    lr

  .size Reset_Handler, .-Reset_Handler

  .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b     Infinite_Loop
  .size Default_Handler, .-Default_Handler

  .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  /* External interrupts */
  .word WWDG_IRQHandler
  .word PVD_IRQHandler
  .word TAMP_STAMP_IRQHandler
  .word RTC_WKUP_IRQHandler
  .word FLASH_IRQHandler
  .word RCC_IRQHandler
  .word EXTI0_IRQHandler
  .word EXTI1_IRQHandler
  .word EXTI2_IRQHandler
  .word EXTI3_IRQHandler
  .word EXTI4_IRQHandler
  .word DMA1_Stream0_IRQHandler
  .word DMA1_Stream1_IRQHandler
  .word DMA1_Stream2_IRQHandler
  .word DMA1_Stream3_IRQHandler
  .word DMA1_Stream4_IRQHandler
  .word DMA1_Stream5_IRQHandler
  .word DMA1_Stream6_IRQHandler
  .word ADC_IRQHandler
  .word CAN1_TX_IRQHandler
  .word CAN1_RX0_IRQHandler
  .word CAN1_RX1_IRQHandler
  .word CAN1_SCE_IRQHandler
  .word EXTI9_5_IRQHandler
  .word TIM1_BRK_TIM9_IRQHandler
  .word TIM1_UP_TIM10_IRQHandler
  .word TIM1_TRG_COM_TIM11_IRQHandler
  .word TIM1_CC_IRQHandler
  .word TIM2_IRQHandler
  .word TIM3_IRQHandler
  .word TIM4_IRQHandler
  .word I2C1_EV_IRQHandler
  .word I2C1_ER_IRQHandler
  .word I2C2_EV_IRQHandler
  .word I2C2_ER_IRQHandler
  .word SPI1_IRQHandler
  .word SPI2_IRQHandler
  .word USART1_IRQHandler
  .word USART2_IRQHandler
  .word USART3_IRQHandler
  .word EXTI15_10_IRQHandler
  .word RTC_Alarm_IRQHandler
  .word OTG_FS_WKUP_IRQHandler
  .word TIM8_BRK_TIM12_IRQHandler
  .word TIM8_UP_TIM13_IRQHandler
  .word TIM8_TRG_COM_TIM14_IRQHandler
  .word TIM8_CC_IRQHandler
  .word DMA1_Stream7_IRQHandler
  .word FMC_IRQHandler
  .word SDIO_IRQHandler
  .word TIM5_IRQHandler
  .word SPI3_IRQHandler
  .word UART4_IRQHandler
  .word UART5_IRQHandler
  .word TIM6_DAC_IRQHandler
  .word TIM7_IRQHandler
  .word DMA2_Stream0_IRQHandler
  .word DMA2_Stream1_IRQHandler
  .word DMA2_Stream2_IRQHandler
  .word DMA2_Stream3_IRQHandler
  .word DMA2_Stream4_IRQHandler
  .word 0
  .word 0
  .word CAN2_TX_IRQHandler
  .word CAN2_RX0_IRQHandler
  .word CAN2_RX1_IRQHandler
  .word CAN2_SCE_IRQHandler
  .word OTG_FS_IRQHandler
  .word DMA2_Stream5_IRQHandler
  .word DMA2_Stream6_IRQHandler
  .word DMA2_Stream7_IRQHandler
  .word USART6_IRQHandler
  .word I2C3_EV_IRQHandler
  .word I2C3_ER_IRQHandler
  .word OTG_HS_EP1_OUT_IRQHandler
  .word OTG_HS_EP1_IN_IRQHandler
  .word OTG_HS_WKUP_IRQHandler
  .word OTG_HS_IRQHandler
  .word DCMI_IRQHandler
  .word 0
  .word HASH_RNG_IRQHandler
  .word FPU_IRQHandler
  .word UART7_IRQHandler
  .word UART8_IRQHandler
  .word SPI4_IRQHandler
  .word SPI5_IRQHandler
  .word SPI6_IRQHandler
  .word SAI1_IRQHandler
  .word 0
  .word 0
  .word 0
  .word SAI2_IRQHandler
  .word QUADSPI_IRQHandler
  .word CEC_IRQHandler
  .word SPDIF_RX_IRQHandler
  .word FMPI2C1_EV_IRQHandler
  .word FMPI2C1_ER_IRQHandler

  /* Weak aliases for all handlers → Default_Handler */
  .macro WEAK_IRQ name
  .weak \name
  .thumb_set \name, Default_Handler
  .endm

  WEAK_IRQ NMI_Handler
  WEAK_IRQ HardFault_Handler
  WEAK_IRQ MemManage_Handler
  WEAK_IRQ BusFault_Handler
  WEAK_IRQ UsageFault_Handler
  WEAK_IRQ SVC_Handler
  WEAK_IRQ DebugMon_Handler
  WEAK_IRQ PendSV_Handler
  WEAK_IRQ SysTick_Handler
  WEAK_IRQ WWDG_IRQHandler
  WEAK_IRQ PVD_IRQHandler
  WEAK_IRQ TAMP_STAMP_IRQHandler
  WEAK_IRQ RTC_WKUP_IRQHandler
  WEAK_IRQ FLASH_IRQHandler
  WEAK_IRQ RCC_IRQHandler
  WEAK_IRQ EXTI0_IRQHandler
  WEAK_IRQ EXTI1_IRQHandler
  WEAK_IRQ EXTI2_IRQHandler
  WEAK_IRQ EXTI3_IRQHandler
  WEAK_IRQ EXTI4_IRQHandler
  WEAK_IRQ DMA1_Stream0_IRQHandler
  WEAK_IRQ DMA1_Stream1_IRQHandler
  WEAK_IRQ DMA1_Stream2_IRQHandler
  WEAK_IRQ DMA1_Stream3_IRQHandler
  WEAK_IRQ DMA1_Stream4_IRQHandler
  WEAK_IRQ DMA1_Stream5_IRQHandler
  WEAK_IRQ DMA1_Stream6_IRQHandler
  WEAK_IRQ ADC_IRQHandler
  WEAK_IRQ CAN1_TX_IRQHandler
  WEAK_IRQ CAN1_RX0_IRQHandler
  WEAK_IRQ CAN1_RX1_IRQHandler
  WEAK_IRQ CAN1_SCE_IRQHandler
  WEAK_IRQ EXTI9_5_IRQHandler
  WEAK_IRQ TIM1_BRK_TIM9_IRQHandler
  WEAK_IRQ TIM1_UP_TIM10_IRQHandler
  WEAK_IRQ TIM1_TRG_COM_TIM11_IRQHandler
  WEAK_IRQ TIM1_CC_IRQHandler
  WEAK_IRQ TIM2_IRQHandler
  WEAK_IRQ TIM3_IRQHandler
  WEAK_IRQ TIM4_IRQHandler
  WEAK_IRQ I2C1_EV_IRQHandler
  WEAK_IRQ I2C1_ER_IRQHandler
  WEAK_IRQ I2C2_EV_IRQHandler
  WEAK_IRQ I2C2_ER_IRQHandler
  WEAK_IRQ SPI1_IRQHandler
  WEAK_IRQ SPI2_IRQHandler
  WEAK_IRQ USART1_IRQHandler
  WEAK_IRQ USART2_IRQHandler
  WEAK_IRQ USART3_IRQHandler
  WEAK_IRQ EXTI15_10_IRQHandler
  WEAK_IRQ RTC_Alarm_IRQHandler
  WEAK_IRQ OTG_FS_WKUP_IRQHandler
  WEAK_IRQ TIM8_BRK_TIM12_IRQHandler
  WEAK_IRQ TIM8_UP_TIM13_IRQHandler
  WEAK_IRQ TIM8_TRG_COM_TIM14_IRQHandler
  WEAK_IRQ TIM8_CC_IRQHandler
  WEAK_IRQ DMA1_Stream7_IRQHandler
  WEAK_IRQ FMC_IRQHandler
  WEAK_IRQ SDIO_IRQHandler
  WEAK_IRQ TIM5_IRQHandler
  WEAK_IRQ SPI3_IRQHandler
  WEAK_IRQ UART4_IRQHandler
  WEAK_IRQ UART5_IRQHandler
  WEAK_IRQ TIM6_DAC_IRQHandler
  WEAK_IRQ TIM7_IRQHandler
  WEAK_IRQ DMA2_Stream0_IRQHandler
  WEAK_IRQ DMA2_Stream1_IRQHandler
  WEAK_IRQ DMA2_Stream2_IRQHandler
  WEAK_IRQ DMA2_Stream3_IRQHandler
  WEAK_IRQ DMA2_Stream4_IRQHandler
  WEAK_IRQ CAN2_TX_IRQHandler
  WEAK_IRQ CAN2_RX0_IRQHandler
  WEAK_IRQ CAN2_RX1_IRQHandler
  WEAK_IRQ CAN2_SCE_IRQHandler
  WEAK_IRQ OTG_FS_IRQHandler
  WEAK_IRQ DMA2_Stream5_IRQHandler
  WEAK_IRQ DMA2_Stream6_IRQHandler
  WEAK_IRQ DMA2_Stream7_IRQHandler
  WEAK_IRQ USART6_IRQHandler
  WEAK_IRQ I2C3_EV_IRQHandler
  WEAK_IRQ I2C3_ER_IRQHandler
  WEAK_IRQ OTG_HS_EP1_OUT_IRQHandler
  WEAK_IRQ OTG_HS_EP1_IN_IRQHandler
  WEAK_IRQ OTG_HS_WKUP_IRQHandler
  WEAK_IRQ OTG_HS_IRQHandler
  WEAK_IRQ DCMI_IRQHandler
  WEAK_IRQ HASH_RNG_IRQHandler
  WEAK_IRQ FPU_IRQHandler
  WEAK_IRQ UART7_IRQHandler
  WEAK_IRQ UART8_IRQHandler
  WEAK_IRQ SPI4_IRQHandler
  WEAK_IRQ SPI5_IRQHandler
  WEAK_IRQ SPI6_IRQHandler
  WEAK_IRQ SAI1_IRQHandler
  WEAK_IRQ SAI2_IRQHandler
  WEAK_IRQ QUADSPI_IRQHandler
  WEAK_IRQ CEC_IRQHandler
  WEAK_IRQ SPDIF_RX_IRQHandler
  WEAK_IRQ FMPI2C1_EV_IRQHandler
  WEAK_IRQ FMPI2C1_ER_IRQHandler
