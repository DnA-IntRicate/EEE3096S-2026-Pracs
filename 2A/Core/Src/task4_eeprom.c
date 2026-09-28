/**
  ******************************************************************************
  * @file    task4_eeprom.c
  * @brief   TASK 4 : EEPROM DRIVER
  *
  * Build the driver on spi_transfer() - no pre-written EEPROM library.
  * Command values, transaction order and status bits come from the EEPROM
  * datasheet; the address format for this board is in board_config.h.
  *
  * Write completion must be decided from the EEPROM's status register. A fixed
  * delay is not acceptable as proof that a write finished. Blocking while you
  * poll is allowed in Task 4.
  ******************************************************************************
  */

#include "prac2a.h"

volatile uint16_t eeprom_test_addr        = (uint16_t)EEPROM_ADDR_A;
volatile uint8_t  eeprom_test_byte        = (uint8_t)TEST_BYTE_B;

volatile uint8_t  eeprom_status_before    = 0u;
volatile uint8_t  eeprom_status_after     = 0u;
volatile uint8_t  eeprom_read_value       = 0u;
volatile uint8_t  eeprom_verify_ok        = 0u;
volatile uint32_t eeprom_write_wait_ms    = 0u;
volatile uint32_t eeprom_timeout_count    = 0u;

/* ==========================================================================
 * Driver
 * ========================================================================== */

// Bit A8 travels inside bit 3 of the instruction byte and A7..A0 follow as one address byte.
static void ee_send_cmd_addr(uint8_t cmd, uint16_t address)
{
    if (address & 0x100u)
        cmd |= 0x08u; // A8 in instruction bit 3

    (void)spi_transfer(cmd);
    (void)spi_transfer((uint8_t)(address & 0xFFu));
}

uint8_t eeprom_read_status(void)
{
    // Select the device, send the read-status instruction, clock one
    // more byte to receive the status, deselect, and return it.
    //
    // TODO: Be ready to explain why you have to SEND a byte in order to RECEIVE one.

    uint8_t sr;

    eeprom_cs_low();
    (void)spi_transfer(EE_CMD_RDSR);
    sr = spi_transfer(0x00u);
    eeprom_cs_high();

    return sr;
}

void eeprom_write_enable(void)
{
    // Send the write-enable instruction as one complete transaction.
    //
    // TODO: Check the datasheet: at what point does the write
    //       enable latch actually get set? That decides whether this could
    //       share a transaction with the write itself.

    eeprom_cs_low();
    (void)spi_transfer(EE_CMD_WREN);
    eeprom_cs_high();   // WEL is only set on the rising edge
}

void eeprom_write_byte(uint16_t address, uint8_t value)
{
    uint32_t start;
    if (address >= EEPROM_SIZE_BYTES)
        return;

    // Enable writes. Then send the write instruction, the address
    // (EEPROM_ADDR_BYTES bytes, most significant byte first) and the
    // data byte, and deselect.
    //
    // TODO: From the datasheet: at what moment does the EEPROM actually start writing?

    // WREN must be its own transaction: the latch sets when CS goes high.
    eeprom_write_enable();
    eeprom_cs_low();
    ee_send_cmd_addr(EE_CMD_WRITE, address);
    (void)spi_transfer(value);
    eeprom_cs_high(); // Internal write cycle start

     // Wait for the write to finish by POLLING THE STATUS REGISTER.
     // Bound the loop with EEPROM_WRITE_TIMEOUT_MS so a missing device cannot hang the board.
     //
     // TODO: Count timeouts in eeprom_timeout_count, and record how long
     //       the write took in eeprom_write_wait_ms - your report needs that figure.
    start = HAL_GetTick();

    // Poll RDY until the internal write cycle ends
    while (eeprom_read_status() & EE_SR_RDY)
    {
        if ((uint32_t)(HAL_GetTick() - start) >= EEPROM_WRITE_TIMEOUT_MS)
        {
            eeprom_timeout_count++;
            break;
        }
    }

    eeprom_write_wait_ms = (uint32_t)(HAL_GetTick() - start);
}

uint8_t eeprom_read_byte(uint16_t address)
{
    if (address >= EEPROM_SIZE_BYTES)
        return 0u;

     uint8_t v;


    // Select, send the read instruction and the address, clock one
    // more byte to receive the data, deselect, and return it.

    eeprom_cs_low();
    ee_send_cmd_addr(EEPROM_CMD_READ, address);
    v = spi_transfer(0x00u); // Dummy out
    eeprom_cs_high();

    return v;
}

/* ==========================================================================
 * LEDs
 * ========================================================================== */

void leds_write_byte(uint8_t v)
{
    // Show v on PB0..PB7 (LED7..LED0 = bit 7..bit 0).
    // Do it without disturbing PB10..PB15 on the same port - those
    // carry the status LEDs, CS and the SPI pins.
    GPIOB->BSRR = (uint32_t)v | ((uint32_t)(uint8_t)(~v) << 16);
}

/* ==========================================================================
 * High-level paths
 * ========================================================================== */

void eeprom_read_only_path(void)
{
    /* Given. Reads, never writes - this is what makes the persistence test
     * meaningful, and it protects the EEPROM's finite write endurance. */
    eeprom_status_before = eeprom_read_status();
    eeprom_read_value    = eeprom_read_byte(eeprom_test_addr);
    eeprom_verify_ok     = (eeprom_read_value == eeprom_test_byte) ? 1u : 0u;
    leds_write_byte(eeprom_read_value);
    status_leds_show(eeprom_verify_ok ? STATUS_PASS : STATUS_FAIL);
}

void eeprom_write_verify_path(void)
{
    // The complete Task 4 sequence, using eeprom_test_addr and eeprom_test_byte.
    eeprom_status_before = eeprom_read_status();

    eeprom_write_byte(eeprom_test_addr, eeprom_test_byte);

    eeprom_status_after  = eeprom_read_status();
    eeprom_read_value    = eeprom_read_byte(eeprom_test_addr);
    eeprom_verify_ok     = (eeprom_read_value == eeprom_test_byte) ? 1 : 0;

    leds_write_byte(eeprom_read_value);
    status_leds_show(eeprom_verify_ok ? STATUS_PASS : STATUS_FAIL);
}

/* ==========================================================================
 * RUN_TASK 4 (and 5) - given
 *
 *   reset / power-up : read-only path - shows the stored byte, writes nothing,
 *                      so a power-cycle proves the byte persisted
 *   PA0              : one complete eeprom_write_verify_path()
 *   eeprom_read_loop_enable = 1 (Live Expressions): repeat a READ-ONLY
 *                      transaction every 50 ms for a steady scope trace.
 *                      It never writes.
 * ========================================================================== */

#define EEPROM_READ_LOOP_MS  50u

volatile uint8_t eeprom_read_loop_enable = 0u;

static uint32_t rd_last = 0u;

void task4_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
    board_io_init();
    eeprom_read_only_path();
}

void task4_loop(uint32_t now)
{
    task1_gpio_update(now);
    read_inputs(now);

    if (btn_start_edge)
    {
        btn_start_edge = 0u;
        eeprom_write_verify_path();     /* one press, one write */
    }
    btn_abort_edge = 0u;                /* PA3 is not used in Task 4 */

    if (eeprom_read_loop_enable && ((uint32_t)(now - rd_last) >= EEPROM_READ_LOOP_MS))
    {
        rd_last = now;
        eeprom_read_only_path();
    }
}
