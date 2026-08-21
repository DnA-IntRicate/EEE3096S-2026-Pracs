/*
 * Task 1: The Golden Measure on the PC
 *
 * Integer square root of a 32-bit unsigned input x: the largest integer
 * whose square does
 * not exceed x.  Golden version uses double precision
 * arithmetic and the standard library square root.
 *
 * Prediction (written before
 * running):
 *   On a ~3 GHz x86-64 CPU, sqrt + floor + conversion is roughly
 *   20..40 instructions.  Estimate: 40 instructions * 0.33
 * ns/instr
 *   ~= 13 ns per call.  One call alone cannot be timed reliably because
 *   clock_gettime resolution is ~10..50 ns and
 * scheduling noise is larger
 *   than the call itself, so we loop and divide.
 *
 * Build:   gcc -O2 -o golden_measure golden_measure.c
 * -lm
 * Run:     ./golden_measure
 */

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static const uint32_t inputs[10] = { 0, 1, 15, 16, 4095, 65535, 123456789, 987654321, 4294836225u, 4294967295u };

static uint32_t golden_isqrt(uint32_t x)
{
    return (uint32_t)floor(sqrt((double)x));
}

static double timestamp_us(void)
{
    struct timespec ts;

#if defined(__linux__)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#elif defined(_WIN32)
    timespec_get(&ts, TIME_UTC);
#endif

    return (double)ts.tv_sec * 1000000.0 + (double)ts.tv_nsec / 1000.0;
}

// Hand check: r^2 <= x < (r+1)^2, written out in full.
static int hand_check(uint32_t x, uint32_t r)
{
    uint64_t r_squared    = (uint64_t)r * r;
    uint64_t next_squared = (uint64_t)(r + 1) * (r + 1);

    return r_squared <= x && x < next_squared;
}

static double time_n_calls(long reps)
{
    double start;
    double end;

    // NOTE: Volatile prevents the compiler from eliminating the calls
    // since this variable is unused
    volatile uint32_t sink = 0;

    start = timestamp_us();
    for (long i = 0; i < reps; ++i)
        for (int j = 0; j < 10; ++j)
            sink ^= golden_isqrt(inputs[j]);

    end = timestamp_us();

    // Return average time per golden_isqrt() call in nanoseconds.
    // Elapsed time is in microseconds, hence:
    // us / calls * 1000 = ns / call
    return ((end - start) * 1000.0) / ((double)reps * 10.0);
}

int main(void)
{
    const long repetitions[] = { 1000, 1000000 };

    printf("Golden Measure\n");
    printf("==============\n\n");

    // Task 2 needs these ten outputs
    printf("Golden outputs:\n");

    for (int i = 0; i < 10; ++i)
    {
        uint32_t result = golden_isqrt(inputs[i]);
        int valid       = hand_check(inputs[i], result);

        printf("sqrt(%10u) = %10u [%s]\n", inputs[i], result, valid ? "PASS" : "FAIL");
    }

    printf("\nTiming:\n");

    for (size_t i = 0; i < sizeof(repetitions) / sizeof(repetitions[0]); ++i)
    {
        double ns_per_call = time_n_calls(repetitions[i]);
        printf("%10ld repetitions: %10.3f ns/call\n", repetitions[i], ns_per_call);
    }

    return 0;
}
