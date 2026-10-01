/**
  ******************************************************************************
  * @file    task2_spi_config.c
  * @brief   TASK 2 : CONFIGURE THE HARDWARE SPI PERIPHERAL
  *
  * Configure the SPI peripheral entirely through its registers. Not allowed:
  * HAL_SPI_*, LL SPI transfer functions, CubeMX-generated SPI configuration,
  * GPIO bit-banging. Every register write you add should cite the RM0091
  * register it comes from.
  ******************************************************************************
  */

#include "prac2a.h"

volatile uint32_t dbg_gpiob_moder      = 0u;
volatile uint32_t dbg_gpiob_afrh       = 0u;
volatile uint32_t dbg_spi_cr1          = 0u;
volatile uint32_t dbg_spi_cr2          = 0u;
volatile uint32_t dbg_spi_sr           = 0u;
volatile uint32_t dbg_sck_hz_predicted = 0u;

void eeprom_spi_init(void)
{
    /* 2.4 Clocks: GPIOB in AHBENR (IOPBEN, bit 18); SPI2 in APB1ENR (SPI2EN, bit 14) */
    RCC->AHBENR  |= RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    (void)RCC->APB1ENR;                       /* clock settle */

    /* 2.5 CS: set the output level HIGH first, then make it an output,
     * so the pin never glitches low and selects the EEPROM. */
    EE_SPI_GPIO->BSRR  = EE_CS_MASK;
    EE_SPI_GPIO->MODER = (EE_SPI_GPIO->MODER & ~MODER2_MASK(EE_PIN_CS))
                       |  MODER2(EE_PIN_CS, 1u);          /* 01 = output */

    /* 2.6 SCK/MISO/MOSI: MODER = 10 (alternate function), AFRH = AF0 */
    EE_SPI_GPIO->MODER = (EE_SPI_GPIO->MODER
                          & ~(MODER2_MASK(EE_PIN_SCK) | MODER2_MASK(EE_PIN_MISO) | MODER2_MASK(EE_PIN_MOSI)))
                       |  MODER2(EE_PIN_SCK, 2u) | MODER2(EE_PIN_MISO, 2u) | MODER2(EE_PIN_MOSI, 2u);

    EE_SPI_GPIO->AFR[1] = (EE_SPI_GPIO->AFR[1]
                           & ~(AFRH4_MASK(EE_PIN_SCK) | AFRH4_MASK(EE_PIN_MISO) | AFRH4_MASK(EE_PIN_MOSI)))
                        |  AFRH4(EE_PIN_SCK, EE_SPI_AF) | AFRH4(EE_PIN_MISO, EE_SPI_AF)
                        |  AFRH4(EE_PIN_MOSI, EE_SPI_AF);

    /* 2.7 High speed (11) on SCK and MOSI; pull-up (01) on MISO */
    EE_SPI_GPIO->OSPEEDR |= MODER2(EE_PIN_SCK, 3u) | MODER2(EE_PIN_MOSI, 3u);
    EE_SPI_GPIO->PUPDR    = (EE_SPI_GPIO->PUPDR & ~MODER2_MASK(EE_PIN_MISO))
                          |  MODER2(EE_PIN_MISO, 1u);

    /* 2.8 CR2 (before SPE): DS = 0111 (8-bit), FRXTH = 1 (RXNE after 8 bits) */
    EE_SPI->CR2 = (7UL << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;

    /* 2.9 CR1: master, BR, mode 0, MSB first, software NSS with SSI = 1.
     * SPE is deliberately left clear here. */
    EE_SPI->CR1 = SPI_CR1_MSTR
                | (EE_SPI_BR << SPI_CR1_BR_Pos)
                | SPI_CR1_SSM | SPI_CR1_SSI;            /* CPOL=CPHA=LSBFIRST=0 */

    task5_fault_hook();                                  /* leave exactly here */

    /* 2.10 Enable */
    EE_SPI->CR1 |= SPI_CR1_SPE;

    dbg_gpiob_moder      = EE_SPI_GPIO->MODER;
    dbg_gpiob_afrh       = EE_SPI_GPIO->AFR[1];
    dbg_spi_cr1          = EE_SPI->CR1;
    dbg_spi_cr2          = EE_SPI->CR2;
    dbg_spi_sr           = EE_SPI->SR;
    dbg_sck_hz_predicted = EE_SCK_HZ_PREDICTED;
}

/* ==========================================================================
 * RUN_TASK 2 - given
 * Configures SPI and then does nothing else, so you can inspect the selected
 * SPI peripheral and GPIOB in the SFR view (or the dbg_* variables) while the
 * program runs. PC13 keeps toggling so you can see the program has not hung.
 * ========================================================================== */

void task2_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
}

void task2_loop(uint32_t now)
{
    task1_gpio_update(now);
}
