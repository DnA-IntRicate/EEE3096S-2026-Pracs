// EEE3095S Practical 1A
// Task 6: Software PWM via Timer interrupts
//
// Adam Foflonker (FFLADA001)
// Sibahle Ntando Mndebele (MNDSIB016)

#include <main.h>
#include <stm32f0xx.h>
#include <stdint.h>

// Calculation: 8 MHz / ((7 + 1) * (99 + 1)) = 10 kHz
#define TIM16_PSC_VALUE 7
#define TIM16_ARR_VALUE 99 // Counts 100 ticks

void SystemClock_Config(void);
static void GPIO_Init(void);
static void TIM16_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    TIM16_Init();

    // NOTE: ADC_Init enables and starts continuous conversion

    // All PWM generation happens in the ISR; main loop remains free
    while (1)
    {
    }
}

/**
  @brief System Clock Configuration (HSI 8 MHz)
*/
void SystemClock_Config(void)
{
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
    {
    }

    LL_RCC_HSI_Enable();
    while (LL_RCC_HSI_IsReady() != 1)
    {
    }

    LL_RCC_HSI_SetCalibTrimming(16);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
    {
    }

    LL_SetSystemCoreClock(8000000);

    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK)
        Error_Handler();
}

static void GPIO_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure PB4 as a general purpose output, push-pull, medium speed, no pull-up/pull-down
    GPIO_InitTypeDef gpio_init = {};
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpio_init.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    // Ensure PB4 starts low
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
}

static void TIM16_Init(void)
{
    __HAL_RCC_TIM16_CLK_ENABLE();

    // Set Prescaler and ARR values for 10 kHz interrupt frequency
    TIM16->PSC = TIM16_PSC_VALUE;
    TIM16->ARR = TIM16_ARR_VALUE;

    // Clear pending update flags
    TIM16->SR &= ~TIM_SR_UIF;

    // Enable the update interrupt
    TIM16->DIER |= TIM_DIER_UIE;

    // Configure NVIC priority and enable TIM16 interrupt
    NVIC_SetPriority(TIM16_IRQn, 2);
    NVIC_EnableIRQ(TIM16_IRQn);

    // Enable the counter to start the interrupt stream
    TIM16->CR1 |= TIM_CR1_CEN;
}

void TIM16_IRQHandler(void)
{
    static uint8_t counter = 0;

    // Check if the update interrupt flag (UIF) is set
    if (TIM16->SR & TIM_SR_UIF)
    {
        // Clear the update interrupt flag
        TIM16->SR &= ~TIM_SR_UIF;

        // Implement 30% duty cycle logic
        if (counter < 30)
            GPIOB->BSRR = GPIO_BSRR_BS_4; // Set PB4 HIGH (30 ticks = 3.0 ms)
        else
            GPIOB->BSRR = GPIO_BSRR_BR_4; // Set PB4 LOW  (70 ticks = 7.0 ms)

        // Reset period after 100 ticks (10.0 ms)
        counter = (counter + 1) % 100;
    }
}

/**
  @brief Error handler
*/
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
