// EEE3095S Practical 1A
//
// Adam Foflonker (FFLADA001)
// Sibahle Ntando Mndebele (MNDSIB016)

/* Includes ------------------------------------------------------------------*/
#include <main.h>
#include <stm32f0xx.h>
#include <stdint.h>

/* ---------- Function prototypes ---------- */
static void GPIO_Init(void);
static void ADC_Init(void);
static void write_led_byte(uint8_t pattern);

int main(void)
{
    HAL_Init();
    GPIO_Init();
    ADC_Init();

    // NOTE: ADC_Init enables and starts continuous conversion

    /* Everything happens in the ISR */
    while (1)
    {
    }
}

/* ---------- GPIO Configuration ---------- */
static void GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init = {};
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    // Configure PA5 as an analog input
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pin = GPIO_PIN_5;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    // Configure LEDs (PB0..PB7) as outputs
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pin = GPIO_PIN_All & 0xFF;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    // Clear GPIOB's ODR
    write_led_byte(0);
}

/* ---------- ADC Configuration ---------- */
static void ADC_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();

    // Ensure ADC is disabled, then run calibration
    if (ADC1->CR & ADC_CR_ADEN)
    {
        // Used in case the ADC is enabled already
        ADC1->CR |= ADC_CR_ADDIS;
        while (ADC1->CR & ADC_CR_ADEN)
        {
        }
    }

    // Start ADC calibration and wait for it to finish
    ADC1->CR |= ADC_CR_ADCAL;
    while (ADC1->CR & ADC_CR_ADCAL)
    {
    }

    // 12-bit resolution
    ADC1->CFGR1 &= ~ADC_CFGR1_RES;

    // Right-align
    ADC1->CFGR1 &= ~ADC_CFGR1_ALIGN;

    // Continuous conversion
    ADC1->CFGR1 |= ADC_CFGR1_CONT;

    // Software trigger
    ADC1->CFGR1 &= ~(ADC_CFGR1_EXTSEL | ADC_CFGR1_EXTEN);

    // Select channel 5 (PA5)
    ADC1->CHSELR = ADC_CHSELR_CHSEL5;

    // Configure sampling time
    ADC1->SMPR |= ADC_SMPR_SMP_0 | ADC_SMPR_SMP_1 | ADC_SMPR_SMP_2;

    // Enable end-of-conversion interrupt
    ADC1->IER |= ADC_IER_EOCIE;
    NVIC_SetPriority(ADC1_COMP_IRQn, 1);
    NVIC_EnableIRQ(ADC1_COMP_IRQn);

    // Enable ADC and wait for it to be ready
    ADC1->CR |= ADC_CR_ADEN;
    while ((ADC1->ISR & ADC_ISR_ADRDY) == 0)
    {
    }

    // Start ADC conversion
    ADC1->CR |= ADC_CR_ADSTART;
}

/* ---------- Interrupt Service Routine ---------- */
void ADC1_COMP_IRQHandler(void)
{
    if (ADC1->ISR & ADC_ISR_EOC)
    {
        // Read the raw 12-bit ADC value from the data register (DR)
        uint32_t adc_val = ADC1->DR;

        // Scale the raw value proportionally to the number of LEDs (0 - 8)
        // NOTE: Max value storeable in 12 bits is 4095
        uint8_t leds_to_light = (uint8_t)((adc_val * 8) / 4095);

        // Build the bar-graph bit pattern using bitwise shifting
        uint8_t output_pattern = 0;
        if (leds_to_light == 0)
            output_pattern = 0;
        else if (leds_to_light >= 8)
            output_pattern = 0xFF;
        else
            output_pattern = (uint8_t)((1 << leds_to_light) - 1);

        write_led_byte(output_pattern);
    }
}

/* ---------- Helper: write an 8-bit pattern to PB0-PB7 ---------- */
static void write_led_byte(uint8_t pattern)
{
    uint32_t odr = GPIOB->ODR;
    odr &= ~(0xFFU);
    odr |= pattern;
    GPIOB->ODR = odr;
}
