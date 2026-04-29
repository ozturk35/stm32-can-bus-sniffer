#include "stm32f4xx.h"

/* 180 MHz after SystemClock_Config() in main.c configures PLL */
uint32_t SystemCoreClock = 16000000U;

const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};

void SystemInit(void)
{
    /* FPU: enable CP10 and CP11 */
    SCB->CPACR |= (3UL << 10*2) | (3UL << 11*2);

    /* Reset RCC to default */
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR = 0x00000000U;
    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->PLLCFGR = 0x24003010U;
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CIR = 0x00000000U;

    /* Vector table at FLASH base */
    SCB->VTOR = FLASH_BASE;
}

void SystemCoreClockUpdate(void)
{
    uint32_t tmp, pllvco, pllp, pllsource, pllm;

    tmp = RCC->CFGR & RCC_CFGR_SWS;
    switch (tmp) {
    case 0x00U: SystemCoreClock = HSI_VALUE; break;
    case 0x04U: SystemCoreClock = HSE_VALUE; break;
    case 0x08U:
        pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) >> 22U;
        pllm      = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;
        if (pllsource != 0U)
            pllvco = (uint32_t)((((uint64_t)HSE_VALUE * ((uint64_t)((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6U))) / (uint64_t)pllm));
        else
            pllvco = (uint32_t)((((uint64_t)HSI_VALUE * ((uint64_t)((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6U))) / (uint64_t)pllm));
        pllp = ((((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >> 16U) + 1U) * 2U);
        SystemCoreClock = pllvco / pllp;
        break;
    default: SystemCoreClock = HSI_VALUE; break;
    }

    tmp = AHBPrescTable[(RCC->CFGR & RCC_CFGR_HPRE) >> 4U];
    SystemCoreClock >>= tmp;
}
