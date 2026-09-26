/**
 ******************************************************************************
 * @file    task1_gpio.c
 * @brief   TASK 1 : MEMORY-MAPPED GPIO ACCESS
 *
 * Drive PC13 as a digital output using EXPLICIT volatile register pointers,
 * built from addresses you find in RM0091:
 *
 *     register address = peripheral base address + register offset
 *
 * Do not use HAL_GPIO_Init(), HAL_GPIO_WritePin() or HAL_GPIO_TogglePin().
 * You must be able to show where every base and offset came from.
 ******************************************************************************
 */

#include "prac2a.h"

/* Peripheral base addresses: the memory map in RM0091 section 2.
 *
 * Memory map table:
 *  - RCC sits in the AHB peripheral block, base 0x4002 1000.
 *  - Ports are spaced 0x400 apart: GPIOA = 0x4800 0000, GPIOB = 0x4800 0400, GPIOC = 0x4800 0800 ...
 */
#define RCC_BASE_ADDR     0x40021000UL
#define GPIOC_BASE_ADDR   0x48000800UL

// Register offsets: the register map at the end of the RCC chapter and of the GPIO chapter of RM0091.
#define RCC_AHBENR_OFFSET 0x14UL
#define GPIO_MODER_OFFSET 0x00UL
#define GPIO_ODR_OFFSET   0x14UL
#define GPIO_BSRR_OFFSET  0x18UL
#define GPIO_BRR_OFFSET   0x28UL

// GPIOC Pins
#define GPIOC_PIN_13       13u
#define GPIOC_PIN_13_MODER (GPIOC_PIN_13 * 2u)

/* One pointer is declared for you to show the pattern. Be ready to explain
 * what `volatile` does here, and what can go wrong without it.
 */
static volatile uint32_t* const pRCC_AHBENR = (volatile uint32_t*)(RCC_BASE_ADDR + RCC_AHBENR_OFFSET);

// GPIOC pointers
static volatile uint32_t* const pGPIOC_MODER = (volatile uint32_t*)(GPIOC_BASE_ADDR + GPIO_MODER_OFFSET);
static volatile uint32_t* const pGPIOC_ODR   = (volatile uint32_t*)(GPIOC_BASE_ADDR + GPIO_ODR_OFFSET);
static volatile uint32_t* const pGPIOC_BSRR  = (volatile uint32_t*)(GPIOC_BASE_ADDR + GPIO_BSRR_OFFSET);
static volatile uint32_t* const pGPIOC_BRR   = (volatile uint32_t*)(GPIOC_BASE_ADDR + GPIO_BRR_OFFSET);

/* Once TODO 1.1 and 1.2 are done, UNCOMMENT these checks. They
 * compare your addresses with the CMSIS device header, so a wrong
 * base or offset fails the build and names the culprit. Fix your
 * address - do not edit the right-hand side.
 * (They are commented out only so the project builds before you
 * start, letting you check your toolchain first.)
 */
_Static_assert(RCC_BASE_ADDR == RCC_BASE, "RCC base mismatch");
_Static_assert(GPIOC_BASE_ADDR == GPIOC_BASE, "GPIOC base mismatch");
_Static_assert(RCC_BASE_ADDR + RCC_AHBENR_OFFSET == (uint32_t)(uintptr_t)&RCC->AHBENR, "AHBENR offset");
_Static_assert(GPIOC_BASE_ADDR + GPIO_MODER_OFFSET == (uint32_t)(uintptr_t)&GPIOC->MODER, "MODER offset");
_Static_assert(GPIOC_BASE_ADDR + GPIO_ODR_OFFSET == (uint32_t)(uintptr_t)&GPIOC->ODR, "ODR offset");
_Static_assert(GPIOC_BASE_ADDR + GPIO_BSRR_OFFSET == (uint32_t)(uintptr_t)&GPIOC->BSRR, "BSRR offset");
_Static_assert(GPIOC_BASE_ADDR + GPIO_BRR_OFFSET == (uint32_t)(uintptr_t)&GPIOC->BRR, "BRR offset");

volatile uint32_t task1_half_period_ms = 5u;
volatile uint32_t task1_toggle_count   = 0u;

static uint32_t t1_last = 0u;

void task1_gpio_init(void)
{
    /* Enable the GPIOC peripheral clock in RCC_AHBENR. Find the bit. Use a read-modify-write so clocks other
     * code has already enabled are kept.
     *
     * GPIOPCEN (I/O port C enable) is bit 19 of AHBENR.
     *
     * If we foeget to enable the clock signal:
     *  The GPIOC block's clock gate stays closed, so the peripheral is unpowered/unclocked.
     *  Writes to GPIOC->MODER/ODR/BSRR either read back as reset values and silently have no effect,
     *  or (on some cores) fault - the pin just never moves, making it look like the physical register
     *  write is incorrect.
     */
    *pRCC_AHBENR |= (1 << 19);

    // Put PC13 into general purpose output mode.
    *pGPIOC_MODER &= ~(3 << GPIOC_PIN_13_MODER);
    *pGPIOC_MODER |= 1 << GPIOC_PIN_13_MODER;

    // Drive PC13 to a known starting level (RESET).
    *pGPIOC_BRR = 1 << GPIOC_PIN_13;
}

void task1_gpio_update(uint32_t now)
{
    if ((uint32_t)(now - t1_last) < task1_half_period_ms)
        return;

    t1_last = now;

    /* Toggle PC13: read its present level, then drive the opposite
     * level. There are set-only and reset-only registers as well as ODR.
     *
     * We read the current output level from ODR, which reflects what we last commanded.
     * To change it we use BSRR, because BSRR is atomic and glitch-free: a single write can set (lower 16
     * bits) and/or reset (upper 16 bits) an individual pin without a read-modify-write on ODR.
     *
     * Doing 'ODR ^= ...' directly would also work logically, but it's a read-modify-write on a
     * register that other pins/peripherals might also be modifying concurrently (e.g. from an ISR),
     * so it's not atomic.
     */
    if ((*pGPIOC_ODR & (1 << GPIOC_PIN_13)))
        *pGPIOC_BSRR = (1 << (GPIOC_PIN_13 + 16u));  // Currently high -> drive low (RESET)
    else
        *pGPIOC_BSRR = (1 << GPIOC_PIN_13);          // Currently low -> drive high (SET)

    task1_toggle_count++;
}

/* ==========================================================================
 * RUN_TASK 1 - given
 *
 * ========================================================================== */

void task1_setup(void)
{
    task1_gpio_init();
}

void task1_loop(uint32_t now)
{
    task1_gpio_update(now);
}
