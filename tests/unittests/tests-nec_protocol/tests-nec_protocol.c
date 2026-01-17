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

#include "nec_protocol.h"
#include "tests-nec_protocol.h"

static nec_protocol_context_t _nec_ctx;

static void set_up(void)
{
    memset(&_nec_ctx, 0, sizeof(_nec_ctx));
    _nec_ctx.current_state = STATE_IDLE;
}

static void tear_down(void)
{
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
    /* Start sequence: RISING edge after 9ms */
    handle_event(EVENT_RISING, START_HIGH_TIME_US, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_START, _nec_ctx.current_state);

    /* Continue with 4.5ms low */
    handle_event(EVENT_FALLING, START_LOW_TIME_US, &_nec_ctx);
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
    handle_event(EVENT_RISING, 5000, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_START, _nec_ctx.current_state);

    /* FALLING edge with incorrect timing - should go back to IDLE */
    handle_event(EVENT_FALLING, 2000, &_nec_ctx);
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
    handle_event(EVENT_FALLING, RECV_HIGH_TIME_US, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, _nec_ctx.current_state);

    /* RISING edge with zero bit timing */
    handle_event(EVENT_RISING, ZERO_LOW_TIME_US, &_nec_ctx);
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
    handle_event(EVENT_FALLING, RECV_HIGH_TIME_US, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, _nec_ctx.current_state);

    /* RISING edge with one bit timing */
    handle_event(EVENT_RISING, ONE_LOW_TIME_US, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, _nec_ctx.current_state);
    TEST_ASSERT_EQUAL_INT(1, _nec_ctx.bits_received);

    /* Bit should be 1 (set) */
    TEST_ASSERT_EQUAL_INT(1, (_nec_ctx.current_msg->data[0] & 0x01));
}

/**
 * Test: Receiving multiple bits in sequence
 */
static void test_receive_multiple_bits(void)
{
    uint32_t i;
    uint32_t expected_bits[] = {0, 1, 1, 0, 1, 0, 0, 1};
    uint32_t expected_byte = 0x5A; /* 01011010 in binary (bits reversed) */

    /* Set up for receiving */
    _nec_ctx.current_state = STATE_RECEIVE;
    _nec_ctx.current_msg = &_nec_ctx.msg_buffer.msg[0];
    _nec_ctx.current_msg->len = 0;
    _nec_ctx.current_msg->data[0] = 0;
    _nec_ctx.bits_received = 0;

    /* Receive 8 bits */
    for (i = 0; i < 8; i++) {
        /* FALLING edge (high time) */
        handle_event(EVENT_FALLING, RECV_HIGH_TIME_US, &_nec_ctx);

        /* RISING edge with bit-specific timing */
        uint32_t bit_timing = (expected_bits[i] == 1) ? ONE_LOW_TIME_US : ZERO_LOW_TIME_US;
        handle_event(EVENT_RISING, bit_timing, &_nec_ctx);

        TEST_ASSERT_EQUAL_INT(i + 1, _nec_ctx.bits_received);
    }

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
    handle_event(EVENT_FALLING, RECV_HIGH_TIME_US, &_nec_ctx);

    /* RISING edge with invalid timing (too long) */
    handle_event(EVENT_RISING, 3000, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, _nec_ctx.current_state);
}

/**
 * Test: Timing tolerance (TIMING_ACCURACY_US)
 */
static void test_timing_tolerance(void)
{
    /* Test with timing within tolerance (slightly above START_HIGH_TIME_US) */
    handle_event(EVENT_RISING, START_HIGH_TIME_US + 5, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_START, _nec_ctx.current_state);

    /* Set up for next transition */
    _nec_ctx.current_state = STATE_START;

    /* Test with timing within tolerance for START_LOW */
    handle_event(EVENT_FALLING, START_LOW_TIME_US + 8, &_nec_ctx);
    TEST_ASSERT_EQUAL_INT(STATE_RECEIVE, _nec_ctx.current_state);
}

/**
 * Test: Timing tolerance exceeded
 */
static void test_timing_tolerance_exceeded(void)
{
    /* Test with timing exceeding tolerance */
    handle_event(EVENT_RISING, START_HIGH_TIME_US + TIMING_ACCURCY_US + 1, &_nec_ctx);
    
    /* Since the guard fails, should fall through to default transition */
    /* Next we try a FALLING event */
    handle_event(EVENT_FALLING, START_LOW_TIME_US, &_nec_ctx);
    
    /* Should return to IDLE since the previous START sequence was invalid */
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, _nec_ctx.current_state);
}

/**
 * Test: Falling edge in IDLE state (should stay in IDLE)
 */
static void test_idle_falling_edge(void)
{
    TEST_ASSERT_EQUAL_INT(STATE_IDLE, _nec_ctx.current_state);
    
    handle_event(EVENT_FALLING, 1000, &_nec_ctx);
    
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
    TEST_ASSERT(!check_timing(duration, expected));

    /* Within tolerance */
    TEST_ASSERT(!check_timing(duration + 5, expected));

    /* Outside tolerance */
    TEST_ASSERT(check_timing(duration + TIMING_ACCURCY_US + 1, expected));
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
    };

    EMB_UNIT_TESTCALLER(nec_protocol_tests, set_up, tear_down, fixtures);

    return (Test *)&nec_protocol_tests;
}

void tests_nec_protocol(void)
{
    TESTS_RUN(tests_nec_protocol_tests());
}

/** @} */
