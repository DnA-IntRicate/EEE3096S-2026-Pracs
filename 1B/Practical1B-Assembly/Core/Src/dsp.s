/*
 * dsp.s
 * EEE3096S 2026 - Practical 1B, Task 4
 * Cycle-counted ADC to DAC loop with a 45 degree phase delay
 *
 * Student 1: Adam Foflonker (FFLADA001)
 * Student 2: Sibahle Ntando Mndebele (MNDSIB016)
 */

    .syntax unified
    .thumb
    .cpu    cortex-m0
    .fpu    softvfp

    .global DSP_Loop
    .type   DSP_Loop, %function

@ ---------------------------------------------------------------------------
@ Peripheral addresses
@ ---------------------------------------------------------------------------
    .equ ADC_DR,      0x40012440
    .equ DAC_DHR12R1, 0x40007408

    .section .text.DSP_Loop, "ax", %progbits

@ --- Timing Derivation -----------------------------------------------------
@ TODO: Document all this in the report.
@
@ Input frequency: f = 1 kHz
@ CPU frequency: fCPU = 8 MHz
@ 1 CPU Cycle = 125 ns
@ Period: T = 1 / 1000 = 1 ms
@ Required phase delay: 45 degrees = 1/8 cycle; Tdelay = 1 ms / 8 = 125 us
@ Required cycles: 125 us * 8 cycles/us = 1000 cycles
@
@ Loop cycle count:
@   LDR R2, [R0]       = 2
@   STR R2, [R1]       = 2
@   MOVS R3, #248    = 1
@
@ Inner loop:
@   SUBS R3, #1        = 1
@   BNE delay_loop     = 3 cycles when taken
@                      = 1 cycle when not taken
@
@ For 248 iterations:
@   SUBS:
@       248 * 1       = 248 cycles
@   BNE taken:
@       247 * 3       = 741 cycles
@
@   BNE not taken:
@       1 * 1         = 1 cycle
@
@   Inner loop total:
@       248 + 741 + 1 = 990 cycles
@
@ Remaining cycles:
@   1000 - (2 + 2 + 1 + 990 + 2) = 3 cycles
@
@ Therefore we can insert three NOPs.
@
@ Complete iteration:
@   LDR         2
@   STR         2
@   MOVS        1
@   delay loop  990
@   NOP (3)     3
@   B loop      2
@
@   TOTAL       1000 cycles
@
@ Delay: 1000 cycles / 8 MHz = 125 us
@ Phase: 125 us / 1 ms * 360 degrees = 45 degrees
@ Loop/sample frequency: 1 / 125 us = 8 kHz
@
@ Therefore the DAC produces 8 samples per 1 kHz input cycle.
@ ---------------------------------------------------------------------------

@ ===========================================================================
@ ENTRY POINT
@ ===========================================================================
DSP_Loop:
    @ Setup registers outside the timed loop
    LDR R0, =ADC_DR
    LDR R1, =DAC_DHR12R1

loop:
    @ --- SAMPLE AND OUTPUT ------------------------------------------------
    @ Read the current ADC conversion from the Data Register.
    LDR R2, [R0]

    @ Write the value straight out to the DAC Data Register.
    STR R2, [R1]

    @ --- DELAY SETUP ------------------------------------------------------
    @ 248 Iterations.
    MOVS R3, #248

delay_loop:
    @ --- INNER LOOP -------------------------------------------------------
    SUBS R3, #1
    BNE delay_loop

    NOP
    NOP
    NOP

    @ --- REPEAT -----------------------------------------------------------
    B loop

    .size DSP_Loop, .-DSP_Loop
