#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "embUnit/embUnit.h"

#include "ir_fsm.h"
#include "tests-infrared.h"
#include "tsrb.h"
#include "ztimer.h"
#include "ztimer/mock.h"

static uint8_t buffer[100]={0};
static tsrb_t recv_buff;
static ir_fsm_state_t fsm;
static ztimer_mock_t mock_timer;


static void set_up(void)
{
    ztimer_mock_init(&mock_timer, 32);
    tsrb_init(&recv_buff, buffer, sizeof(buffer));
    tsrb_clear(&recv_buff);

    fsm = ir_fsm_create(&recv_buff, IR_DEFAULT_TIMING);
}

static void test_fsm_create(void)
{
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);
    TEST_ASSERT_EQUAL_INT(0, fsm.current_bit);
    TEST_ASSERT_EQUAL_INT(0, fsm.current_byte);
    TEST_ASSERT_EQUAL_INT(&recv_buff, fsm.recv_buffer);
    TEST_ASSERT(tsrb_empty(fsm.recv_buffer));
}

/**
 * Test: Valid NEC start sequence (9ms high, 4.5ms low)
 * Should transition from IDLE -> START -> RECEIVE
 */
static void test_valid_start_sequence(void)
{   
    /* Start sequence: RISING edge */
    ir_fsm_handle_event(EVENT_RISING, 0, &fsm);
    
    /* Start sequence: RISING edge after 9ms */
    ir_fsm_handle_event(EVENT_FALLING, fsm.timing.start_high_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_START, fsm.current_state);

    /* Continue with 4.5ms low */
    ir_fsm_handle_event(EVENT_RISING, fsm.timing.start_low_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);
}

/**
 * Test: Invalid start sequence with wrong timing
 * Should return to IDLE if timing is incorrect
 */
static void test_invalid_start_timing(void)
{
    /* RISING edge with incorrect timing (too short) */
    ir_fsm_handle_event(EVENT_RISING, 5000, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_START, fsm.current_state);

    /* FALLING edge with incorrect timing - should go back to IDLE */
    ir_fsm_handle_event(EVENT_FALLING, 2000, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);
}

/**
 * Test: Receiving a logic 0 bit (560us low time)
 */
static void test_receive_logic_zero(void)
{
    /* Set up for receiving */
    fsm.current_state = STATE_RECEIVE;

    /* FALLING edge with normal timing */
    ir_fsm_handle_event(EVENT_FALLING, fsm.timing.recv_high_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);

    /* RISING edge with zero bit timing */
    ir_fsm_handle_event(EVENT_RISING, fsm.timing.zero_low_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);
    TEST_ASSERT_EQUAL_INT(0, fsm.current_byte);
    TEST_ASSERT_EQUAL_INT(1, fsm.current_bit);
}

/**
 * Test: Receiving a logic 1 bit (1687us low time)
 */
static void test_receive_logic_one(void)
{
    /* Set up for receiving */
    fsm.current_state = STATE_RECEIVE;

    /* FALLING edge with normal timing */
    ir_fsm_handle_event(EVENT_FALLING, fsm.timing.recv_high_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);

    /* RISING edge with one bit timing */
    ir_fsm_handle_event(EVENT_RISING, fsm.timing.one_low_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);
    TEST_ASSERT_EQUAL_INT(1, fsm.current_bit);

    /* Bit should be 1 (set) */
    TEST_ASSERT_EQUAL_INT(0b10000000, fsm.current_byte);
}

/**
 * Test: Receiving multiple bits in sequence
 */
static void test_receive_multiple_bits(void)
{
    uint32_t i;
    uint32_t expected_bits[] = {1,0,0,1,0,1,1,0};
    uint32_t expected_byte = 0x96; /* 1001 0110 in binary */

    /* Set up for receiving */
    fsm.current_state = STATE_RECEIVE;

    /* Receive 8 bits */
    for (i = 0; i < 8; i++) {
        
        TEST_ASSERT_EQUAL_INT(i %8, fsm.current_bit);
        
        /* FALLING edge (high time) */
        ir_fsm_handle_event(EVENT_FALLING, fsm.timing.recv_high_time_us, &fsm);
        
        /* RISING edge with bit-specific timing */
        uint32_t bit_timing = (expected_bits[i] == 1) ? fsm.timing.one_low_time_us : fsm.timing.zero_low_time_us;
        ir_fsm_handle_event(EVENT_RISING, bit_timing, &fsm);
        
    }
    
    ir_fsm_handle_event(EVENT_FALLING, fsm.timing.recv_high_time_us, &fsm);
    /* wait for timeout */
    ztimer_mock_advance(&mock_timer, 2);
    /* Verify the received byte */
    uint8_t received_byte = tsrb_get_one(fsm.recv_buffer);
    TEST_ASSERT_EQUAL_INT(expected_byte, received_byte);
}

/**
 * Test: Invalid bit timing returns to IDLE
 */
static void test_invalid_bit_timing(void)
{
    /* Set up for receiving */
    fsm.current_state = STATE_RECEIVE;

    /* FALLING edge with normal timing */
    ir_fsm_handle_event(EVENT_FALLING, fsm.timing.recv_high_time_us, &fsm);

    /* RISING edge with invalid timing (too long) */
    ir_fsm_handle_event(EVENT_RISING, 3000, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);
}

/**
 * Test: Timing tolerance (TIMING_ACCURACY_US)
 */
static void test_timing_tolerance(void)
{
    /* Test with timing within tolerance (slightly above START_HIGH_TIME_US) */
    ir_fsm_handle_event(EVENT_RISING, fsm.timing.start_high_time_us + 5, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_START, fsm.current_state);

    /* Set up for next transition */
    fsm.current_state = STATE_START;

    /* Test with timing within tolerance for START_LOW */
    ir_fsm_handle_event(EVENT_RISING, fsm.timing.start_low_time_us + 8, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);
}

/**
 * Test: Timing tolerance exceeded
 */
static void test_timing_tolerance_exceeded(void)
{
    /* Test with timing exceeding tolerance */
    ir_fsm_handle_event(EVENT_RISING, fsm.timing.start_high_time_us + fsm.timing.timing_tollerance_us + 1, &fsm);
    
    /* Since the guard fails, should fall through to default transition */
    /* Next we try a FALLING event */
    ir_fsm_handle_event(EVENT_FALLING, fsm.timing.start_low_time_us, &fsm);
    
    /* Should return to IDLE since the previous START sequence was invalid */
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);
}

/**
 * Test: Falling edge in IDLE state (should stay in IDLE)
 */
static void test_idle_falling_edge(void)
{   
    ir_fsm_handle_event(EVENT_FALLING, 1000, &fsm);
    
    /* Should remain in IDLE */
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);
}

static Test *tests_ir_fsm_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_fsm_create),
        new_TestFixture(test_valid_start_sequence),
        new_TestFixture(test_invalid_start_timing),
        new_TestFixture(test_receive_logic_zero),
        new_TestFixture(test_receive_logic_one),
        new_TestFixture(test_receive_multiple_bits),
        new_TestFixture(test_invalid_bit_timing),
        new_TestFixture(test_timing_tolerance),
        new_TestFixture(test_timing_tolerance_exceeded),
        new_TestFixture(test_idle_falling_edge),
    };

    EMB_UNIT_TESTCALLER(ir_fsm_tests, set_up, NULL, fixtures);

    return (Test *)&ir_fsm_tests;
}

void tests_infrared(void)
{
    TESTS_RUN(tests_ir_fsm_tests());
}

/** @} */
