/**
  ******************************************************************************
  * @file    task6_fsm.c
  * @brief   TASK 6 : NON-BLOCKING EEPROM TRANSACTION STATE MACHINE
  *
  * Restructure the Task 4 transaction so the main loop never stops:
  *
  *   request -> write-enable -> write -> wait for EEPROM
  *           -> read-back -> verify -> result
  *
  * Rules (handout, Task 6):
  *   - No HAL_Delay(), and no software busy-wait for the EEPROM's internal
  *     write cycle. You may use HAL_GetTick() to decide when the next status
  *     check is due.
  *   - Each call does a small amount of work, updates the state, and returns.
  *   - Do not put the whole transaction inside one blocking function called
  *     from the loop.
  *   - While a transaction is in progress the loop must still respond to PA3.
  *
  * Controls: PA0 starts, PA3 aborts. PB0..PB7 show the last byte read, PB11
  * (green) a successful verification, PB10 (red) a failed one.
  ******************************************************************************
  */

#include "prac2a.h"

typedef enum {
    EE_IDLE = 0,
    EE_WRITE_PREP,    /* device not busy? then send WREN            */
    EE_WRITE_TX,      /* WRITE + addr + data, CS rising edge = go   */
    EE_BUSY_WAIT,     /* not yet time to poll: return immediately   */
    EE_BUSY_CHECK,    /* one RDSR: RDY=1 -> wait again, 0 -> read   */
    EE_READ_TX,
    EE_VERIFY,
    EE_SUCCESS,
    EE_FAIL
} ee_state_t;

enum { RES_NONE = 0, RES_PASS, RES_FAIL };

#define EE_POLL_MS  1u

volatile uint8_t ee_state     = EE_IDLE;
volatile uint8_t ee_last_read = 0u;
volatile uint8_t ee_use_fsm   = 1u;

static uint8_t  ee_result = RES_NONE;
static uint16_t fsm_addr;
static uint8_t  fsm_data;
static uint32_t fsm_t_start;
static uint32_t fsm_next_poll;

static uint8_t ee_due(uint32_t now)      { return ((int32_t)(now - fsm_next_poll) >= 0); }
static uint8_t ee_timed_out(uint32_t now){ return ((uint32_t)(now - fsm_t_start) >= EEPROM_WRITE_TIMEOUT_MS); }

static void ee_abort(void)
{
    eeprom_cs_high();                                   /* deselect                */
    while (EE_SPI->SR & SPI_SR_RXNE)                    /* drain the receive FIFO  */
        (void)*((volatile uint8_t *)&EE_SPI->DR);
    ee_result = RES_NONE;
    ee_state  = EE_IDLE;
}

void update_eeprom_state_machine(uint32_t now)
{
    uint8_t active = (ee_state != EE_IDLE) && (ee_state != EE_SUCCESS) && (ee_state != EE_FAIL);

    /* PA3 is looked at first, on every call */
    if (btn_abort_edge)
    {
        btn_abort_edge = 0u;
        btn_start_edge = 0u;
        if (ee_state != EE_IDLE) ee_abort();
        return;
    }

    /* PA0: starts from idle/result states, ignored (not queued) while active */
    if (btn_start_edge)
    {
        btn_start_edge = 0u;
        if (!active)
        {
            fsm_addr      = eeprom_test_addr;
            fsm_data      = eeprom_test_byte;
            fsm_t_start   = now;
            fsm_next_poll = now;
            ee_result     = RES_NONE;
            ee_state      = EE_WRITE_PREP;
        }
        return;
    }

    switch (ee_state)
    {
    case EE_WRITE_PREP:
        if (!ee_due(now)) break;
        if (eeprom_read_status() & EEPROM_SR_RDY)       /* still busy from earlier */
        {
            if (ee_timed_out(now)) { ee_result = RES_FAIL; ee_state = EE_FAIL; }
            else                     fsm_next_poll = now + EE_POLL_MS;
        }
        else
        {
            eeprom_write_enable();
            ee_state = EE_WRITE_TX;
        }
        break;

    case EE_WRITE_TX:
        eeprom_cs_low();
        ee_send_cmd_addr(EEPROM_CMD_WRITE, fsm_addr);
        (void)spi_transfer(fsm_data);
        eeprom_cs_high();                               /* write cycle starts      */
        fsm_t_start   = now;
        fsm_next_poll = now + EE_POLL_MS;
        ee_state      = EE_BUSY_WAIT;
        break;

    case EE_BUSY_WAIT:
        if (ee_due(now)) ee_state = EE_BUSY_CHECK;
        break;

    case EE_BUSY_CHECK:
        if (!(eeprom_read_status() & EEPROM_SR_RDY))    /* finished per status reg */
        {
            eeprom_write_wait_ms = (uint32_t)(now - fsm_t_start);
            ee_state = EE_READ_TX;
        }
        else if (ee_timed_out(now))                     /* never became ready      */
        {
            eeprom_timeout_count++;
            ee_result = RES_FAIL;
            ee_state  = EE_FAIL;
        }
        else
        {
            fsm_next_poll = now + EE_POLL_MS;
            ee_state      = EE_BUSY_WAIT;
        }
        break;

    case EE_READ_TX:
        ee_last_read = eeprom_read_byte(fsm_addr);
        ee_state     = EE_VERIFY;
        break;

    case EE_VERIFY:
        if (ee_last_read == fsm_data) { ee_result = RES_PASS; ee_state = EE_SUCCESS; }
        else                          { ee_result = RES_FAIL; ee_state = EE_FAIL;    }
        break;

    default:            /* EE_IDLE, EE_SUCCESS, EE_FAIL: hold until PA0/PA3 */
        break;
    }
}

void update_outputs(void)
{
    leds_write_byte(ee_last_read);
    switch (ee_result)
    {
    case RES_PASS: status_leds_show(STATUS_PASS); break;
    case RES_FAIL: status_leds_show(STATUS_FAIL); break;
    default:       status_leds_show(STATUS_OFF);  break;   /* running or aborted */
    }
}
/* ==========================================================================
 * RUN_TASK 6 - given
 *
 * The main loop has exactly the shape the handout asks for:
 *     read_inputs();  update_eeprom_state_machine();  update_outputs();
 *
 * Set ee_use_fsm = 0 in Live Expressions to run the Task 4 blocking path on
 * PA0 instead - useful when you explain why yours is non-blocking. PC13 keeps
 * toggling as a heartbeat; watch it on the scope in both modes.
 * ========================================================================== */

void task6_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
    board_io_init();

    eeprom_read_only_path();
    ee_last_read = eeprom_read_value;

    /* TODO 6.4  Your state machine starts in its idle state. Make sure the
     *           boot-time result (eeprom_verify_ok) still shows on the status
     *           LEDs, so a reset still shows green for the persistence test. */

    ee_state  = EE_IDLE;
    ee_result = eeprom_verify_ok ? RES_PASS : RES_FAIL;   /* boot result stays on the LEDs */
}

void task6_loop(uint32_t now)
{
    task1_gpio_update(now);
    read_inputs(now);

    if (ee_use_fsm)
    {
        update_eeprom_state_machine(now);
        update_outputs();
    }
    else
    {
        if (btn_start_edge)
        {
            btn_start_edge = 0u;
            eeprom_write_verify_path();     /* Task 4: blocks for the write */
            ee_last_read = eeprom_read_value;
        }
        btn_abort_edge = 0u;
    }
}
