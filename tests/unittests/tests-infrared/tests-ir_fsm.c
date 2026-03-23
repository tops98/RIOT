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

    fsm = ir_fsm_create(&recv_buff, IR_DEFAULT_TIMING, &mock_timer.super);
}

static void test_fsm_create(void)
{
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);
    TEST_ASSERT_EQUAL_INT(0, fsm.current_bit);
    TEST_ASSERT_EQUAL_INT(0, fsm.current_byte);
    TEST_ASSERT_EQUAL_INT(&recv_buff, fsm.recv_buffer);
    TEST_ASSERT(tsrb_empty(fsm.recv_buffer));
}


static void test_idle_state(void)
{
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);
    
    /* Falling  shoudl be ignored*/
    ir_fsm_handle_event(EVENT_FALLING, 0, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);

    /* RSING  leads to START*/
    ir_fsm_handle_event(EVENT_RISING, 0, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_START, fsm.current_state);
}

static void test_start_state(void)
{
    fsm.current_state = STATE_START;

    /* Falling  with correct timing*/
    ir_fsm_handle_event(EVENT_FALLING, IR_DEFAULT_TIMING.start_high_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_START, fsm.current_state);

    /* Falling  with incorrect timing*/
    ir_fsm_handle_event(EVENT_FALLING, 0, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);

    
    /* RISING  with correct timing*/
    fsm.current_state = STATE_START;
    fsm.current_bit = 5;
    fsm.current_byte = 3;
    ir_fsm_handle_event(EVENT_RISING, IR_DEFAULT_TIMING.start_low_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);
    /* Verify that the current bit and byte are reset */
    TEST_ASSERT_EQUAL_INT(0, fsm.current_bit);
    TEST_ASSERT_EQUAL_INT(0, fsm.current_byte);

    /* RISING  with incorrect timing*/
    fsm.current_state = STATE_START;
    ir_fsm_handle_event(EVENT_RISING, 0, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);

    /* Timeout Event*/
    /* go first from IDLE to START to arm timer*/
    fsm.current_state = STATE_IDLE;
    ir_fsm_handle_event(EVENT_RISING, 0, &fsm);
    /* Advance mock timer */
    ztimer_mock_advance(&mock_timer, IR_DEFAULT_TIMING.transmission_timeout_ms);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);
}

static void test_receive_state(void)
{
    fsm.current_state = STATE_RECEIVE;

    /* Falling  with correct timing*/
    ir_fsm_handle_event(EVENT_FALLING, IR_DEFAULT_TIMING.recv_high_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);

    /* Falling  with incorrect timing*/
    ir_fsm_handle_event(EVENT_FALLING, 0, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);

    /* RISING  with correct timing for logic 0*/
    fsm.current_state = STATE_RECEIVE;
    ir_fsm_handle_event(EVENT_RISING, IR_DEFAULT_TIMING.zero_low_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);

    /* RISING  with correct timing for logic 1*/
    fsm.current_state = STATE_RECEIVE;
    ir_fsm_handle_event(EVENT_RISING, IR_DEFAULT_TIMING.one_low_time_us, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);

    /* RISING  with incorrect timing*/
    fsm.current_state = STATE_RECEIVE;
    ir_fsm_handle_event(EVENT_RISING, 0, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);

    /* Timeout Event*/
    /* go first from START to RECEIVE to arm timer*/
    fsm.current_state = STATE_START;
    ir_fsm_handle_event(EVENT_RISING, IR_DEFAULT_TIMING.start_low_time_us, &fsm);
    /* Advance mock timer */
    ztimer_mock_advance(&mock_timer, IR_DEFAULT_TIMING.transmission_timeout_ms);
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
 * Test: Timing tolerance (TIMING_ACCURACY_US)
 */
static void test_timing_tolerance(void)
{
    ztimer_now_t duration= 0;

    /* Test with timing within tolerance (above START_HIGH_TIME_US) */
    fsm.current_state = STATE_START;
    duration = fsm.timing.start_low_time_us + fsm.timing.timing_tollerance_us;
    ir_fsm_handle_event(EVENT_RISING, duration, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);
    
    /* Test with timing within tolerance (below START_HIGH_TIME_US) */
    fsm.current_state = STATE_START;
    duration = fsm.timing.start_low_time_us - fsm.timing.timing_tollerance_us;
    ir_fsm_handle_event(EVENT_RISING, duration, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, fsm.current_state);

    /* Test with timing not within tolerance (above START_HIGH_TIME_US)*/
    fsm.current_state = STATE_START;
    duration = fsm.timing.start_low_time_us + fsm.timing.timing_tollerance_us + 1;
    ir_fsm_handle_event(EVENT_RISING, duration, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);
    
    /* Test with timing not within tolerance (below START_HIGH_TIME_US)*/
    fsm.current_state = STATE_START;
    duration = fsm.timing.start_low_time_us - fsm.timing.timing_tollerance_us - 1;
    ir_fsm_handle_event(EVENT_RISING, duration, &fsm);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, fsm.current_state);
}


static Test *tests_ir_fsm_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_fsm_create),

        new_TestFixture(test_idle_state),
        new_TestFixture(test_start_state),
        new_TestFixture(test_receive_state),
        
        new_TestFixture(test_receive_logic_zero),
        new_TestFixture(test_receive_logic_one),
        new_TestFixture(test_receive_multiple_bits),
        new_TestFixture(test_timing_tolerance),
    };

    EMB_UNIT_TESTCALLER(ir_fsm_tests, set_up, NULL, fixtures);

    return (Test *)&ir_fsm_tests;
}

void tests_ir_fsm(void)
{
    TESTS_RUN(tests_ir_fsm_tests());
}

/** @} */
