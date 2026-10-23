/**
 * EEE3096S Practical 3: Voltage Mirror.
 *
 * @author Sibahle Ntando Mndebele (MNDSIB016)
 * @author Adam Foflonker (FFLADA001)
 *
 *
 * @date 21 October 2026
 */
#include "voltage_mirror.h"

#include "board_io.h"
#include "main.h"


/* ---- ADC + DAC configuration -------------------------------- */
bool voltage_mirror_init(void)
{
    if (!board_converters_prepare())
        return false;

    /* ADC1: 12-bit, right-aligned, software-started single conversions,
     * channel 5 only, sampling time 239.5 ADC cycles.
     *
     * DAC channel 1: buffer on, trigger off, initial command 2048, enable.
     * Set ADC1->CFGR1, ADC1->CHSELR, ADC1->SMPR, DAC->DHR12R1
     * and DAC->CR.
     * Finish by returning board_adc_enable(). Remove the false return below. */
    ADC1->CFGR1 = 0u;

    /* Select only channel 5 (PA5). */
    ADC1->CHSELR = ADC_CHSELR_CHSEL5;

    /* Sampling time 239.5 ADC cycles -> SMP = 111b (field is bits 2:0). */
    ADC1->SMPR = ADC_SMPR_SMP;

    /* ---- DAC channel 1: buffer on, trigger off, enable ----
     * DAC_CR reset value is 0x00000000, so BOFF1 = 0 (buffer on) and
     * TEN1 = 0 (software trigger via DHR write) already. Only EN1
     * needs to be set. Write the whole register so nothing stale remains. */
    DAC->CR = DAC_CR_EN1;

    /* Initial mid-scale command so the output does not start at 0 V. */
    DAC->DHR12R1 = 2048u;

    /* Finish: enable ADC and wait (bounded) for ADRDY. */
    return board_adc_enable();
}

/* ---- One bounded ADC read ----------------------------------- */
bool voltage_mirror_read(uint16_t* code)
{
    if (code == NULL)
        return false;

    /* Clear any stale EOC / EOS / OVR using write-one-to-clear.
     * Do NOT read-modify-write these flags; a RMW would re-set them. */
    ADC1->ISR = ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;

    /* Start one conversion (software trigger). */
    ADC1->CR |= ADC_CR_ADSTART;

    /* Bounded wait for EOC. At 14 MHz ADC clock with 239.5-cycle
     * sampling, one conversion is ~18 us. 100000 loop iterations at
     * 48 MHz is ~10 ms worst case, far longer than needed but short
     * enough that a real hardware failure is caught quickly. */
    uint32_t timeout = 100000u;
    while ((ADC1->ISR & ADC_ISR_EOC) == 0u)
    {
        if (--timeout == 0u)
        {
            /* Ask the ADC to stop; do not block waiting for ADSTP. */
            ADC1->CR |= ADC_CR_ADSTP;
            return false;
        }
    }

    /* DR is 12-bit right-aligned; mask defensively. Reading DR clears
     * EOC automatically on this family, but we already cleared it above. */
    *code = (uint16_t)(ADC1->DR & 0x0FFFu);
    return true;
}

/* ---- Bounded DAC write -------------------------------------- */
void voltage_mirror_write(uint16_t code)
{
    /* Limit code to 0..4095 and write DAC->DHR12R1. */

    if (code > MIRROR_MAX_CODE)
        code = MIRROR_MAX_CODE;

    DAC->DHR12R1 = code; /* Right-aligned 12-bit data. */
}

/* ---- Code mapping ------------------------------------------- */
uint16_t voltage_mirror_map(uint16_t code, bool coarse)
{
    /* Limit code to 0..4095. FULL passes it through; COARSE keeps
     * the six most significant bits and clears the six least significant.
     *
     * Retain the 12-bit DAC scale: do not send a 0..63 code directly. */

    if (code > MIRROR_MAX_CODE)
        code = MIRROR_MAX_CODE;

    /* COARSE6: keep the six MSBs, clear the six LSBs.
     * Equivalent to D = 64 * (C / 64). Still a 12-bit DAC command. */
    if (coarse)
        code &= 0x0FC0u;

    return code;
}
