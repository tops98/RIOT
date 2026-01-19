/*
 * SPDX-License-Identifier: LGPL-2.1-only
 */

/**
 * @{
 *
 * @file
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "embUnit/embUnit.h"

#include "ztimer.h"
#include "nec_protocol.h"
#include "tests-nec_protocol.h"

static nec_protocol_context_t _nec_ctx;
static ztimer_now_t last_falling_edge = 0;

static void mock_pulse_sending(uint32_t high_duration_us) {
    ztimer_now_t low_duration = 0;
    ztimer_now_t current_time = ztimer_now(ZTIMER_USEC);
    
    if(last_falling_edge){
        low_duration = current_time - last_falling_edge;
        nec_protocol_handle_event(EVENT_RISING, low_duration, &_nec_ctx);
    }else{
        nec_protocol_handle_event(EVENT_RISING, 0, &_nec_ctx);
    }
    nec_protocol_handle_event(EVENT_FALLING, high_duration_us, &_nec_ctx);
    last_falling_edge = current_time;
}

static void set_up(void)
{
    memset(&_nec_ctx, 0, sizeof(_nec_ctx));
    nec_protocol_init(&_nec_ctx, mock_pulse_sending);
}

static void tear_down(void)
{
    nec_protocol_init(&_nec_ctx, mock_pulse_sending);
    memset(&_nec_ctx, 0, sizeof(_nec_ctx));
}

/**
 * Test: Initial state should be IDLE
 */
static void test_initial_state(void)
{
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, _nec_ctx.current_state);
}

/**
 * Test: Valid NEC start sequence (9ms high, 4.5ms low)
 * Should transition from IDLE -> START -> RECEIVE
 */
static void test_valid_start_sequence(void)
{   
    /* Start sequence: RISING edge */
    nec_protocol_handle_event(EVENT_RISING, 0, &_nec_ctx);
    
    /* Start sequence: RISING edge after 9ms */
    nec_protocol_handle_event(EVENT_FALLING, START_HIGH_TIME_US, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_START, _nec_ctx.current_state);

    /* Continue with 4.5ms low */
    nec_protocol_handle_event(EVENT_RISING, START_LOW_TIME_US, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, _nec_ctx.current_state);

    /* Message should be initialized */
    TEST_ASSERT_NOT_NULL(_nec_ctx.current_msg);
    TEST_ASSERT_EQUAL_INT(0, _nec_ctx.bits_received);
}

/**
 * Test: Invalid start sequence with wrong timing
 * Should return to IDLE if timing is incorrect
 */
static void test_invalid_start_timing(void)
{
    /* RISING edge with incorrect timing (too short) */
    nec_protocol_handle_event(EVENT_RISING, 5000, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_START, _nec_ctx.current_state);

    /* FALLING edge with incorrect timing - should go back to IDLE */
    nec_protocol_handle_event(EVENT_FALLING, 2000, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, _nec_ctx.current_state);
}

/**
 * Test: Receiving a logic 0 bit (560us low time)
 */
static void test_receive_logic_zero(void)
{
    /* Set up for receiving */
    _nec_ctx.current_state = STATE_RECEIVE;
    _nec_ctx.current_msg = &_nec_ctx.msg_buffer.msg[0];
    _nec_ctx.current_msg->len = 0;
    _nec_ctx.bits_received = 0;

    /* FALLING edge with normal timing */
    nec_protocol_handle_event(EVENT_FALLING, RECV_HIGH_TIME_US, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, _nec_ctx.current_state);

    /* RISING edge with zero bit timing */
    nec_protocol_handle_event(EVENT_RISING, ZERO_LOW_TIME_US, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, _nec_ctx.current_state);
    TEST_ASSERT_EQUAL_INT(1, _nec_ctx.bits_received);

    /* Bit should be 0 (not set) */
    TEST_ASSERT_EQUAL_INT(0, (_nec_ctx.current_msg->data[0] & 0x01));
}

/**
 * Test: Receiving a logic 1 bit (1687us low time)
 */
static void test_receive_logic_one(void)
{
    /* Set up for receiving */
    _nec_ctx.current_state = STATE_RECEIVE;
    _nec_ctx.current_msg = &_nec_ctx.msg_buffer.msg[0];
    _nec_ctx.current_msg->len = 0;
    _nec_ctx.current_msg->data[0] = 0;
    _nec_ctx.bits_received = 0;

    /* FALLING edge with normal timing */
    nec_protocol_handle_event(EVENT_FALLING, RECV_HIGH_TIME_US, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, _nec_ctx.current_state);

    /* RISING edge with one bit timing */
    nec_protocol_handle_event(EVENT_RISING, ONE_LOW_TIME_US, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, _nec_ctx.current_state);
    TEST_ASSERT_EQUAL_INT(1, _nec_ctx.bits_received);

    /* Bit should be 1 (set) */
    TEST_ASSERT_EQUAL_INT(0b10000000, (_nec_ctx.current_msg->data[0]));
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
    _nec_ctx.current_state = STATE_RECEIVE;
    _nec_ctx.current_msg = &_nec_ctx.msg_buffer.msg[0];
    _nec_ctx.current_msg->len = 0;
    _nec_ctx.current_msg->data[0] = 0;
    _nec_ctx.bits_received = 0;

    /* Receive 8 bits */
    for (i = 0; i < 8; i++) {
        /* FALLING edge (high time) */
        nec_protocol_handle_event(EVENT_FALLING, RECV_HIGH_TIME_US, &_nec_ctx);
        
        /* RISING edge with bit-specific timing */
        uint32_t bit_timing = (expected_bits[i] == 1) ? ONE_LOW_TIME_US : ZERO_LOW_TIME_US;
        nec_protocol_handle_event(EVENT_RISING, bit_timing, &_nec_ctx);
        
        TEST_ASSERT_EQUAL_INT(i + 1, _nec_ctx.bits_received);
    }
    
    nec_protocol_handle_event(EVENT_FALLING, RECV_HIGH_TIME_US, &_nec_ctx);
    /* wait for timeout */
    ztimer_sleep(ZTIMER_MSEC, 2);
    /* Verify the received byte */
    TEST_ASSERT_EQUAL_INT(expected_byte, _nec_ctx.current_msg->data[0]);
}

/**
 * Test: Invalid bit timing returns to IDLE
 */
static void test_invalid_bit_timing(void)
{
    /* Set up for receiving */
    _nec_ctx.current_state = STATE_RECEIVE;
    _nec_ctx.current_msg = &_nec_ctx.msg_buffer.msg[0];
    _nec_ctx.current_msg->len = 0;
    _nec_ctx.bits_received = 0;

    /* FALLING edge with normal timing */
    nec_protocol_handle_event(EVENT_FALLING, RECV_HIGH_TIME_US, &_nec_ctx);

    /* RISING edge with invalid timing (too long) */
    nec_protocol_handle_event(EVENT_RISING, 3000, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, _nec_ctx.current_state);
}

/**
 * Test: Timing tolerance (TIMING_ACCURACY_US)
 */
static void test_timing_tolerance(void)
{
    /* Test with timing within tolerance (slightly above START_HIGH_TIME_US) */
    nec_protocol_handle_event(EVENT_RISING, START_HIGH_TIME_US + 5, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_START, _nec_ctx.current_state);

    /* Set up for next transition */
    _nec_ctx.current_state = STATE_START;

    /* Test with timing within tolerance for START_LOW */
    nec_protocol_handle_event(EVENT_RISING, START_LOW_TIME_US + 8, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, _nec_ctx.current_state);
}

/**
 * Test: Timing tolerance exceeded
 */
static void test_timing_tolerance_exceeded(void)
{
    /* Test with timing exceeding tolerance */
    nec_protocol_handle_event(EVENT_RISING, START_HIGH_TIME_US + TIMING_ACCURCY_US + 1, &_nec_ctx);
    
    /* Since the guard fails, should fall through to default transition */
    /* Next we try a FALLING event */
    nec_protocol_handle_event(EVENT_FALLING, START_LOW_TIME_US, &_nec_ctx);
    
    /* Should return to IDLE since the previous START sequence was invalid */
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, _nec_ctx.current_state);
}

/**
 * Test: Falling edge in IDLE state (should stay in IDLE)
 */
static void test_idle_falling_edge(void)
{
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, _nec_ctx.current_state);
    
    nec_protocol_handle_event(EVENT_FALLING, 1000, &_nec_ctx);
    
    /* Should remain in IDLE */
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, _nec_ctx.current_state);
}

/**
 * Test: Guard function check_timing
 */
static void test_check_timing_guard(void)
{
    uint32_t duration = 9000;
    uint32_t expected = 9000;

    /* Exact match */
    TEST_ASSERT(check_timing(duration, expected));

    /* Within tolerance */
    TEST_ASSERT(check_timing(duration + 5, expected));
    TEST_ASSERT(check_timing(duration - 5, expected));

    /* Outside tolerance */
    TEST_ASSERT(!check_timing(duration + TIMING_ACCURCY_US + 1, expected));
}

static void test_send_logic_one(void) {
    uint8_t expected_data[] ={0b10000000}; 
    Message buffer;
    nec_protocol_context_t ctx =  _nec_ctx;
    (void)ctx;
    nec_protocol_send(expected_data, 1, &_nec_ctx);
    ztimer_sleep(ZTIMER_MSEC,500);
    TEST_ASSERT_EQUAL_INT(1, message_queue_length(&_nec_ctx.msg_buffer));
    
    message_queue_pop(&_nec_ctx.msg_buffer, &buffer);
    
    TEST_ASSERT_EQUAL_INT(1, buffer.len);
    TEST_ASSERT_EQUAL_INT(expected_data[0], _nec_ctx.msg_buffer.msg[0].data[0]);
}

static void test_send_logic_zero(void) {
    uint8_t expected_data[] ={0b00000000}; 
    Message buffer;
    nec_protocol_context_t ctx =  _nec_ctx;
    (void)ctx;
    nec_protocol_send(expected_data, 1, &_nec_ctx);
    ztimer_sleep(ZTIMER_MSEC,500);
    TEST_ASSERT_EQUAL_INT(1, message_queue_length(&_nec_ctx.msg_buffer));
    
    message_queue_pop(&_nec_ctx.msg_buffer, &buffer);
    
    TEST_ASSERT_EQUAL_INT(1, buffer.len);
    TEST_ASSERT_EQUAL_INT(expected_data[0], _nec_ctx.msg_buffer.msg[0].data[0]);
}

static void test_send_byte(void) {
    uint8_t expected_data[] ={0b11010100}; 
    Message buffer;
    nec_protocol_context_t ctx =  _nec_ctx;
    (void)ctx;
    nec_protocol_send(expected_data, 1, &_nec_ctx);
    ztimer_sleep(ZTIMER_MSEC,500);
    TEST_ASSERT_EQUAL_INT(1, message_queue_length(&_nec_ctx.msg_buffer));
    
    message_queue_pop(&_nec_ctx.msg_buffer, &buffer);
    
    TEST_ASSERT_EQUAL_INT(1, buffer.len);
    TEST_ASSERT_EQUAL_INT(expected_data[0], _nec_ctx.msg_buffer.msg[0].data[0]);
}

static void test_send_multiple_bytes(void) {
    uint8_t expected_data[] ="Hello world"; 
    Message buffer;
    nec_protocol_context_t ctx =  _nec_ctx;
    (void)ctx;
    nec_protocol_send(expected_data, sizeof(expected_data), &_nec_ctx);
    ztimer_sleep(ZTIMER_MSEC,500);
    TEST_ASSERT_EQUAL_INT(1, message_queue_length(&_nec_ctx.msg_buffer));
    
    message_queue_pop(&_nec_ctx.msg_buffer, &buffer);
    
    TEST_ASSERT_EQUAL_INT(sizeof(expected_data), buffer.len);
    TEST_ASSERT_EQUAL_STRING((char*)expected_data, (char*)_nec_ctx.msg_buffer.msg[0].data);
}

static Test *tests_nec_protocol_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_initial_state),
        new_TestFixture(test_valid_start_sequence),
        new_TestFixture(test_invalid_start_timing),
        new_TestFixture(test_receive_logic_zero),
        new_TestFixture(test_receive_logic_one),
        new_TestFixture(test_receive_multiple_bits),
        new_TestFixture(test_invalid_bit_timing),
        new_TestFixture(test_timing_tolerance),
        new_TestFixture(test_timing_tolerance_exceeded),
        new_TestFixture(test_idle_falling_edge),
        new_TestFixture(test_check_timing_guard),
        // these tests are timing dependend and my fail! TODO: refactor with moctimer
        new_TestFixture(test_send_logic_one),
        new_TestFixture(test_send_logic_zero),
        new_TestFixture(test_send_byte),
        new_TestFixture(test_send_multiple_bytes),
    };

    EMB_UNIT_TESTCALLER(nec_protocol_tests, set_up, tear_down, fixtures);

    return (Test *)&nec_protocol_tests;
}

void tests_nec_protocol(void)
{
    TESTS_RUN(tests_nec_protocol_tests());
}

/** @} */
