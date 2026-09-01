/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * EEE3096S 2026 - Practical 1B
 * Tasks 2 and 3: fast integer square root, TIM16 timing, optimization flags
 *
 * Student 1:  Adam Foflonker (FFLADA001)
 * Student 2:  Sibahle Ntando Mndebele (MNDSIB016)
 * Date:       21 Aug 2026
 *
 * Board pins used
 *   PC13 : scope pulse. Driven LOW for the timed section, HIGH otherwise.
 *          Broken out on Header P1.
 *   PB1  : pass or fail indicator. User LED 1. ON means all ten golden
 *          values matched.
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdint.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define PULSE_PIN  13u        /* PC13 */
#define LED_PIN    1u         /* PB1  */

#define TEST_INPUT 987654321u   /* the input named in the Task 2 question */
#define LONG_RUN_N 100u       /* calls in the wrap-around run           */
/* USER CODE END PD */

/* USER CODE BEGIN PV */

// The ten inputs from Task 1. Do not change these.
static const uint32_t golden_inputs[10] = {0u, 1u, 15u, 16u, 4095u, 65535u, 123456789u, 987654321u, 4294836225u, 4294967295u};

// Golden outputs recorded from Task 1
// The firmware self-test below compares against these.
static const uint32_t golden_outputs[10] = {0u, 1u, 3u, 4u, 63u, 255u, 11111u, 31426u, 65535u, 65535u};

/*
 * Results. Keep these volatile so the optimiser leaves them alone at -O1
 * and above. Read them in the STM32CubeIDE Live Expressions view.
 */
volatile uint8_t pass_all          = 0u;   /* 1 means all ten matched      */
volatile uint32_t single_call_span = 0u;   /* timer counts, one call       */
volatile uint32_t long_run_span    = 0u;   /* timer counts, LONG_RUN_N     */
volatile float mean_us_per_call    = 0.0f; /* long run divided by N        */

/* Sink for the return value. Stops the optimiser deleting the call. */
static volatile uint32_t sink = 0u;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static void gpio_init(void);
static void timing_timer_init(void);
static uint32_t isqrt(uint32_t x);
static uint32_t time_one_call(uint32_t x);
static uint32_t time_n_calls(uint32_t x, uint32_t n);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* ---------------------------------------------------------------------------
 * Hardware initialization
 * ------------------------------------------------------------------------ */
static void gpio_init(void)
{
    // Enable the peripheral clock for GPIOC and GPIOB.
    //
    // RCC->AHBENR is the GPIO clock enable register.
    // TODO: Name this register in the report.
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Put PC13 and PB1 into general purpose output mode.
    GPIOC->MODER &= ~(GPIO_MODER_MODER13);
    GPIOC->MODER |= GPIO_MODER_MODER13_0;

    GPIOB->MODER &= ~(GPIO_MODER_MODER1);
    GPIOB->MODER |= GPIO_MODER_MODER1_0;

    // Set the idle states: PC13 HIGH (pulse is active low) and PB1 LOW (LED off until the self-test passes).
    GPIOC->BSRR = (1 << PULSE_PIN);
    GPIOB->BRR  = (1 << LED_PIN);
}

static void timing_timer_init(void)
{
    // Enable the peripheral clock for TIM16
    // TIM16 is on APB2ENR
    __HAL_RCC_TIM16_CLK_ENABLE();

    // HSI = 8 MHz; AHB prescaler  = /1; APB2 prescaler = /1
    // Choose a 1 ms timer tick: 8 MHz / (7 + 1) = 1 MHz
    // Therefore each timer count = 1 us at PSC = 7
    TIM16->PSC = 7;

    // Free-running 16-bit counter.
    // At 1 us/tick this wraps every 65.536 ms.
    TIM16->ARR = UINT16_MAX;

    // Force the prescaler to load with an update event
    TIM16->EGR = TIM_EGR_UG;

    // Enable the counter
    TIM16->CR1 |= TIM_CR1_CEN;
}

/* ---------------------------------------------------------------------------
 * Task 2 core algorithm
 * ------------------------------------------------------------------------ */

/*
 * Helper. Returns non-zero when mid * mid is at or below x.
 *
 * Keep this as a separate function. Task 3 asks you to find one
 * optimisation transformation in the disassembly, and the treatment of
 * this helper at -O2 is the easiest one to spot and name.
 *
 * Note: mid * mid overflows 32 bits for large mid. Promote before you
 * multiply.
 */
static inline uint32_t square_le(uint32_t mid, uint32_t x)
{
    return ((uint64_t)mid * mid) <= x;
}

/*
 * Integer square root. Returns the largest r with r * r <= x, for every
 * x from 0 to 4294967295.
 */
static uint32_t isqrt(uint32_t x)
{
    uint32_t low  = 0;
    uint32_t high = 65535; // Since x is uint32, the sqrt can never exceed 65535, because: 65535^2 = 4294836225 < 2^(32)
    uint32_t res  = 0;

    while (low <= high)
    {
        uint32_t mid = low + (high - low) / 2;
        if (square_le(mid, x))
        {
            res = mid;
            low = mid + 1;
        }
        else
            high = mid - 1;
    }

    return res;
}

/* ---------------------------------------------------------------------------
 * Timing harness
 * ------------------------------------------------------------------------ */

/*
 * Times one call.
 * Drives PC13 LOW for the timed window so the scope pulse and the counter
 * span cover the same code.
 * Returns the elapsed counter span.
 */
static uint32_t time_one_call(uint32_t x)
{
    uint16_t a = 0;
    uint16_t b = 0;

    GPIOC->BRR = (1 << PULSE_PIN);  // PC13 low: pulse starts

    a    = TIM16->CNT;              // Capture the counter into a
    sink = isqrt(x);                // The code under test
    b    = TIM16->CNT;              // Capture the counter into b

    GPIOC->BSRR = (1 << PULSE_PIN); // PC13 high: pulse ends

    // Return the elapsed span.
    // Since TIM16 is 16-bit (and we cast to uint16), unsigned subtraction naturally wraps modulo 65536.
    //
    // For a = 65500; b = 20
    // (65536 - 65500) + 20
    // = 36 + 20
    // = 56
    return (uint16_t)(b - a);
}

/*
 * Times n calls back to back so the counter crosses its overflow more
 * than once. Same arithmetic as the single call version.
 */
static uint32_t time_n_calls(uint32_t x, uint32_t n)
{
    uint16_t a = 0;
    uint16_t b = 0;

    GPIOC->BRR = (1 << PULSE_PIN); // PC13 low: pulse starts
    a          = TIM16->CNT;       // Capture the counter into a

    for (uint32_t i = 0; i < n; i++)
        sink = isqrt(x);

    b           = TIM16->CNT;       // Capture the counter into b
    GPIOC->BSRR = (1 << PULSE_PIN); // PC13 high: pulse ends

    // The unambiguous measurement window is 6.5536 seconds:
    // 1 tick = 1 us; 65536 ticks = 6.5536 seconds.
    //
    // TODO: Pick n such that the total run stays inside this window and state n in the report.

    // Returns the elapsed span
    return (uint16_t)(b - a);
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 */
int main(void)
{
    /* MCU Configuration -------------------------------------------------------*/
    HAL_Init();
    SystemClock_Config();

    /* USER CODE BEGIN 2 */
    gpio_init();
    timing_timer_init();

    // Self-test against the ten golden values from Task 1
    pass_all = 1;
    for (int i = 0; i < 10; i++)
    {
        if (isqrt(golden_inputs[i]) != golden_outputs[i])
        {
            pass_all = 0;
            break;
        }
    }

    // Drive PB1 from pass_all. LED on for a pass, off for a fail.
    GPIOB->BSRR = pass_all ? (1 << LED_PIN) : (1 << (LED_PIN + 16));

    /* USER CODE END 2 */

    /* Infinite loop */
    while (1)
    {
        /* USER CODE BEGIN 3 */

        // Task 2 and Task 3: single call measurement.
        // TIM16 runs at 1 MHz, so each counter tick represents 1 us.
        single_call_span = time_one_call(TEST_INPUT);

        /*
         * Task 2 wrap case: run the long measurement, then work out the mean
         * time per call and confirm it agrees with the single call figure.
         *
         * Comment this out while you place the scope cursors on the single
         * call pulse. Two pulses of very different widths on one pin make the
         * scope trigger jump.
         */
        // long_run_span    = time_n_calls(TEST_INPUT, LONG_RUN_N);
        // mean_us_per_call = (float)long_run_span / (float)LONG_RUN_N;

        // Gap between measurements so the scope has a clean single pulse
        for (volatile int d = 0; d < 100000; d++)
        {
        }
        /* USER CODE END 3 */
    }
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
        Error_Handler();
}

/**
 * @brief  This function is executed in case of error occurrence.
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) { }
#endif /* USE_FULL_ASSERT */

/*
 * ---------------------------------------------------------------------------
 * TASK 3 CHECKLIST. No code changes needed below this line.
 * ---------------------------------------------------------------------------
 *
 * Build this same file four times, once per optimisation level.
 *
 *   Project > Properties > C/C++ Build > Settings > MCU GCC Compiler
 *     > Optimization > Optimization Level
 *
 *   None            -O0
 *   Optimize        -O1
 *   Optimize more   -O2
 *   Optimize size   -Os
 *
 * At every level record:
 *   1. Text size in bytes. Build Analyzer tab, or run
 *        arm-none-eabi-size Debug/Practical1B.elf
 *   2. single_call_span from Live Expressions.
 *   3. The PC13 pulse width from the scope, with cursors.
 *
 * At -O2 also dump the disassembly of your isqrt function:
 *        arm-none-eabi-objdump -d Debug/Practical1B.elf > disasm_O2.txt
 *   or open Debug/Practical1B.list, which the build already produces.
 * Find one transformation the compiler applied, name it, and point at the
 * source lines above it acts on.
 * ---------------------------------------------------------------------------
 */
