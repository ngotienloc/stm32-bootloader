#include "stm32f1xx.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_utils.h"
#include "stm32f1xx_ll_rcc.h"
#include "stm32f1xx_ll_system.h"

void SystemClock_Config(void); 


int main (void){
    SystemClock_Config();

    LL_Init1msTick(SystemCoreClock);

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOC);

    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = LL_GPIO_PIN_13;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    while(1){
        LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_13);
        LL_mDelay(500);
    }
}

void SystemClock_Config(void){
    LL_RCC_HSE_Enable();
    while(LL_RCC_HSE_IsReady() != 1);

    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);

    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE_DIV_1, LL_RCC_PLL_MUL_9);
    LL_RCC_PLL_Enable();
    while(LL_RCC_PLL_IsReady() != 1);
    
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {}

    SystemCoreClock = 72000000;
}

