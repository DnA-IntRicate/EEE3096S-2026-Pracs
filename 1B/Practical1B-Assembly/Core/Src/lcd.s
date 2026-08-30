/*
 * lcd.s
 * EEE3096S 2026 - Practical 1B, Task 5
 * 4-bit bit-banged HD44780 driver, and the level shifter timing fault
 *
 * Pins:
 *  LCD VDD = +5V, Contrast = POT2, R/W = GND (write only).
 *  RS = PC14, E = PC15  (E is wired straight to PC15, no level shifter).
 *  D4 = PB8, D5 = PB9, D6 = PA12, D7 = PA15  (upper 4 data lines, 4-bit mode).
 *
 * Student 1: Adam Foflonker (FFLADA001)
 * Student 2: Sibahle Ntando Mndebele (MNDSIB016)
 * Date:      30 Aug 2026
 */

    .syntax unified
    .thumb
    .cpu    cortex-m0
    .fpu    softvfp

    .global LCD_Run
    .type   LCD_Run, %function

@ ---------------------------------------------------------------------------
@ Register addresses. BSRR is at offset 0x18 from each port base.
@ ---------------------------------------------------------------------------
    .equ RCC_AHBENR, 0x40021014

    @ GPIOA
    .equ GPIOA_BSRR, 0x48000018
    .equ GPIOA_MODER,  0x48000000
    .equ GPIOA_OTYPER, 0x48000004

    @ GPIOB
    .equ GPIOB_BSRR, 0x48000418
    .equ GPIOB_MODER,  0x48000400
    .equ GPIOB_OTYPER, 0x48000404

    @ GPIOC
    .equ GPIOC_BSRR, 0x48000818
    .equ GPIOC_MODER,  0x48000800
    .equ GPIOC_OTYPER, 0x48000804

@ Bit masks (lcd_stm32f0.h) ------------------------------------------------
    .equ RS_SET,    (1 << 14)      @ PC14 high
    .equ RS_RESET,  (1 << 30)      @ PC14 low
    .equ EN_SET,    (1 << 15)      @ PC15 high
    .equ EN_RESET,  (1 << 31)      @ PC15 low
    .equ D7_SET,    (1 << 15)      @ PA15
    .equ D7_RESET,  (1 << 31)
    .equ D6_SET,    (1 << 12)      @ PA12
    .equ D6_RESET,  (1 << 28)
    .equ D5_SET,    (1 << 9)       @ PB9
    .equ D5_RESET,  (1 << 25)
    .equ D4_SET,    (1 << 8)       @ PB8
    .equ D4_RESET,  (1 << 24)

@ HD44780 command bytes (lcd_stm32f0.h) ------------------------------------
    .equ POWER_UP,     0x33
    .equ FOURBIT_MODE, 0x32
    .equ DISPLAY_ON,   0x0C
    .equ TWOLINE_MODE, 0x28
    .equ CLEAR,        0x01
    .equ ENTRY_MODE,   0x06

    .section .text.LCD_Run, "ax", %progbits

@ ===========================================================================
@ ENTRY POINT
@ ===========================================================================
LCD_Run:
    @ Initialize the LCD and display the character 'A.'
    PUSH    {LR}

    @ Wait for the LCD power rail to settle (at least 40ms according to datasheet).
    MOVS    R0, #50         @ (50ms)
    BL      LCD_DelayLong

    @ Call the 4-bit initialization sequence.
    BL      LCD_Init

    @ Write the character 'A' (0x41) to the display.
    MOVS    R0, #0x41       @ 'A'
    BL      LCD_WriteData

hang:
    B    hang

    .size LCD_Run, .-LCD_Run

@ ===========================================================================
@ LCD_Init
@ Puts the controller into 4-bit mode and readies the display.
@ ===========================================================================
    .type LCD_Init, %function
LCD_Init:
    PUSH {LR}

    @ Enable GPIOA/B/C clocks.
    LDR     R0, =RCC_AHBENR
    LDR     R1, [R0]
    LDR     R2, =((1 << 17) | (1 << 18) | (1 << 19))   @ GPIOA/B/C EN
    ORRS    R1, R1, R2
    STR     R1, [R0]

    @ Initialize the LCD pins to push-pull output.
    @ MODER: General-purpose output
    @ OTYPER: Push-pull
    @
    @ GPIOA
    LDR     R0, =GPIOA_MODER                @ PA12 (D6), PA15 (D7)
    LDR     R1, [R0]
    LDR     R2, =((3 << 24) | (3 << 30))    @ Clear both fields
    BICS    R1, R1, R2
    LDR     R2, =((1 << 24) | (1 << 30))    @ Set output
    ORRS    R1, R1, R2
    STR     R1, [R0]
    LDR     R0, =GPIOA_OTYPER
    LDR     R1, [R0]
    LDR     R2, =((1 << 12) | (1 << 15))    @ Push-pull
    BICS    R1, R1, R2
    STR     R1, [R0]
    @ GPIOB
    LDR     R0, =GPIOB_MODER                @ PB8 (D4), PB9 (D5)
    LDR     R1, [R0]
    LDR     R2, =((3 << 16) | (3 << 18))
    BICS    R1, R1, R2
    LDR     R2, =((1 << 16) | (1 << 18))
    ORRS    R1, R1, R2
    STR     R1, [R0]
    LDR     R0, =GPIOB_OTYPER
    LDR     R1, [R0]
    LDR     R2, =((1 << 8) | (1 << 9))
    BICS    R1, R1, R2
    STR     R1, [R0]
    @ GPIOC
    LDR  R0, =GPIOC_MODER                   @ PC14 (RS), PC15 (E)
    LDR  R1, [R0]
    LDR  R2, =((3 << 28) | (3 << 30))
    BICS R1, R1, R2
    LDR  R2, =((1 << 28) | (1 << 30))
    ORRS R1, R1, R2
    STR  R1, [R0]
    LDR  R0, =GPIOC_OTYPER
    LDR  R1, [R0]
    LDR  R2, =((1 << 14) | (1 << 15))
    BICS R1, R1, R2
    STR  R1, [R0]

    @ Should we do the power up settle here?
    @ MOVS    R0, #50     @ (50ms)
    @ BL      LCD_DelayLong

    @ Initialization (from init_LCD)
    @ Order follows the HD44780 4-bit init flowchart.
    MOVS    R0, #POWER_UP
    BL      LCD_WriteCmd
    MOVS    R0, #FOURBIT_MODE
    BL      LCD_WriteCmd
    MOVS    R0, #TWOLINE_MODE
    BL      LCD_WriteCmd
    MOVS    R0, #DISPLAY_ON
    BL      LCD_WriteCmd
    MOVS    R0, #CLEAR
    BL      LCD_WriteCmd

    @ Clear Display (and Return Home) need up to 1.52ms to execute, far
    @ longer than the ~37us other commands need. R/W is tied to GND so
    @ we can't poll the busy flag - without this extra wait, the very
    @ next byte sent (LCD_Run's 'A') lands while the controller is still
    @ busy clearing and gets dropped, which is why nothing shows up.
    MOVS    R0, #2       @ (2ms, comfortably clears the 1.52ms spec)
    BL      LCD_DelayLong
    MOVS    R0, #ENTRY_MODE
    BL      LCD_WriteCmd

    POP {PC}

@ ===========================================================================
@ LCD_WriteCmd   R0 = command byte, RS low
@ LCD_WriteData  R0 = data byte,    RS high
@ Both send the high nibble first, then the low nibble.
@ ===========================================================================
    .type LCD_WriteCmd, %function
LCD_WriteCmd:
    PUSH {R0, LR}

    @ Drive RS (PC14) LOW, then fall through to the shared sender.
    LDR     R1, =GPIOC_BSRR
    LDR     R2, =RS_RESET
    STR     R2, [R1]
    B       LCD_Send8

    .type LCD_WriteData, %function
LCD_WriteData:
    PUSH {R0, LR}

    @ Drive RS (PC14) HIGH, then fall through.
    LDR     R1, =GPIOC_BSRR
    LDR     R2, =RS_SET
    STR     R2, [R1]

LCD_Send8:
    @ Send the upper nibble of R0, pulse Enable,
    @ then the lower nibble of R0, pulse Enable again.
    @ LCD_SendNibble/LCD_Pulse both preserve R1, so we can store the
    @ original byte there across both calls.
    MOVS    R1, R0          @ Stash the full byte
    LSRS    R0, R1, #4      @ Upper nibble -> bits 3:0 (zero-extended)
    BL      LCD_SendNibble
    BL      LCD_Pulse

    MOVS    R0, R1          @ Lower nibble is just bits 3:0 of the byte -
    BL      LCD_SendNibble  @ SendNibble only ever looks at bits 3:0
    BL      LCD_Pulse

    POP {R0, PC}

@ ===========================================================================
@ LCD_SendNibble   R0 bits 3:0 -> the four data lines
@ ===========================================================================
    .type LCD_SendNibble, %function
LCD_SendNibble:
    PUSH {R1, R2, R3, LR}

    @ Map the four bits of R0 onto the four data pins (across 3 ports).
    @   R0 bit 0 -> PB8   (D4)
    @   R0 bit 1 -> PB9   (D5)
    @   R0 bit 2 -> PA12  (D6)
    @   R0 bit 3 -> PA15  (D7)
    @ LSRS R2, R0, #n shifts bit (n-1) of R0 out into the Carry flag, so
    @ we can test each bit without disturbing R0 for the next test.

    @ D4 (PB8), bit 0
    LDR     R1, =GPIOB_BSRR
    LSRS    R2, R0, #1      @ C = bit0
    BCS     1f
    LDR     R3, =D4_RESET
    B       2f
1:  LDR     R3, =D4_SET
2:  STR     R3, [R1]

    @ D5 (PB9), bit 1
    LSRS    R2, R0, #2      @ C = bit1
    BCS     1f
    LDR     R3, =D5_RESET
    B       2f
1:  LDR     R3, =D5_SET
2:  STR     R3, [R1]

    @ D6 (PA12), bit 2
    LDR     R1, =GPIOA_BSRR
    LSRS    R2, R0, #3      @ C = bit2
    BCS     1f
    LDR     R3, =D6_RESET
    B       2f
1:  LDR     R3, =D6_SET
2:  STR     R3, [R1]

    @ D7 (PA15), bit 3
    LSRS    R2, R0, #4      @ C = bit3
    BCS     1f
    LDR     R3, =D7_RESET
    B       2f
1:  LDR     R3, =D7_SET
2:  STR     R3, [R1]

    POP {R1, R2, R3, PC}

@ ===========================================================================
@ LCD_Pulse
@ ===========================================================================
    .type LCD_Pulse, %function
LCD_Pulse:
    PUSH {R0, R1, R2, LR}

    LDR  R0, =GPIOC_BSRR

    @ Set PC15 HIGH.
    LDR     R1, =EN_SET
    STR     R1, [R0]

    @ -----------------------------------------------------------------
    @ THE TIMING FIX
    @ Implement a calculated pad delay here to overcome the RC time
    @ constant of the level shifter and meet the HD44780 hold time requirements.
    @ Show your cycle arithmetic in the comments.
    @ -----------------------------------------------------------------
    @
    @ D4-D7 pass through the board's RC level-shifting network (3.3V ->
    @ 5V); E is wired straight to PC15 so it switches almost instantly,
    @ which means E can go high before the shifted data lines have
    @ finished slewing to a valid logic level. Padding Enable-high also
    @ covers the HD44780's own minimum enable pulse width (PWeh >= 450ns).
    @ LCD_DelayShort costs 3 cycles/iteration @ 48MHz SYSCLK (see its
    @ header): R0 = 5us -> 5 * 16 * 3 = 240 cycles ~= 5us, well clear of
    @ both the RC settle time and PWeh.
    MOVS    R0, #5
    BL      LCD_DelayShort

    @ Set PC15 LOW.
    LDR     R0, =GPIOC_BSRR
    LDR     R1, =EN_RESET
    STR     R1, [R0]

    @ Hold Enable low long enough to meet the LCD cycle time.
    @ R/W is tied to GND so we can never poll the busy flag; instead we
    @ pad generously (50us) to clear the worst-case instruction execution
    @ time (most commands finish inside 37us) before the next nibble pair
    @ can be sent. 50 * 16 * 3 = 2400 cycles ~= 50us @ 48MHz.
    MOVS    R0, #50
    BL      LCD_DelayShort

    POP {R0, R1, R2, PC}

@ ===========================================================================
@ Delay helpers
@ ===========================================================================
    .type LCD_DelayLong, %function
LCD_DelayLong:
    @ Millisecond-scale delay.
    @
    @ R0 = milliseconds to wait (destroyed). Assumes SYSCLK = 48MHz.
    @ Inner loop body: SUBS (1 cycle) + BNE (2 cycles taken / 1 cycles not-taken)
    @                  = 3 cycles per iteration while it keeps looping.
    @ 1ms @ 48MHz = 48000 cycles -> 48000 / 3 = 16000 iterations/ms.
    CMP     R0, #0
    BEQ     2f
1:  LDR     R1, =16000
3:  SUBS    R1, R1, #1
    BNE     3b
    SUBS    R0, R0, #1
    BNE     1b
2:  BX      LR

    .type LCD_DelayShort, %function
LCD_DelayShort:
    @ Microsecond-scale delay.
    @
    @ R0 = microseconds to wait (destroyed). Assumes SYSCLK = 48MHz.
    @ Inner loop body: SUBS (1 cycles) + BNE (2 cycles taken / 1 cycles not-taken)
    @                  = 3 cycles per iteration while it keeps looping.
    @ 1us @ 48MHz = 48 cycles -> 48 / 3 = 16 iterations/us.
    CMP     R0, #0
    BEQ     2f
1:  MOVS    R1, #16
3:  SUBS    R1, R1, #1
    BNE     3b
    SUBS    R0, R0, #1
    BNE     1b
2:  BX      LR
