#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "embUnit/embUnit.h"

#include "debug.h"
#include "tsrb.h"
#include "ir_receiver.h"
#include "ztimer.h"
#include "ztimer/mock.h"
#include "periph/pwm.h"
#include "ir_transmitter.h"
#include "tests-infrared.h"

static uint8_t buffer[128]={0};
static ztimer_mock_t mock_timer_us;
static ztimer_mock_t mock_timer_ms;
static ir_receiver_t receiver;
static ir_transmitter_t transmitter;
static gpio_t recv_gpio;



static void simulate_interrupt(bool edge)
{
    receiver.int_flank = edge;
    receiver.int_timestamp_us = ztimer_now(&mock_timer_us.super);
    sema_post(&receiver.fsm_sema);
}

void pwm_set(pwm_t pwm, uint8_t channel, uint16_t value)
{
    (void)pwm;
    (void)channel;
    simulate_interrupt(value == IR_DEFAULT_PWM_CONF->send_duty_cycle);
}

void ir_transmitter_sleep(ztimer_clock_t *clock, ztimer_now_t timeout) 
{ 
    (void)clock;
    (void)timeout;
    ztimer_mock_advance(&mock_timer_us, timeout);
}

static void wait_for_bytes( int32_t num_bytes, uint8_t timout_ms)
{
    int32_t received = 0;
    ztimer_now_t start = ztimer_now(ZTIMER_MSEC);
    while (ztimer_now(ZTIMER_MSEC) - start < timout_ms)
    {
        received = tsrb_avail(&receiver.recv_buffer);
        if(received >= num_bytes)
        {
          break;  
        }
    }
    TEST_ASSERT_EQUAL_INT(num_bytes, received);
}

static void set_up(void)
{
    /* Setup receiver */
    ztimer_mock_init(&mock_timer_ms, 32);
    ir_receiver_init(&receiver, recv_gpio, buffer, sizeof(buffer));
    receiver.fsm.clock_ms = &mock_timer_ms.super;
    
    /* Setup sender */
    ztimer_mock_init(&mock_timer_us, 32);
    ir_transmitter_init(&transmitter, 0);
    transmitter.clock_us = &mock_timer_us.super;
}

static void test_init_receiver(void)
{
    TEST_ASSERT(NULL != receiver.fsm.recv_buffer);
    TEST_ASSERT(buffer == receiver.recv_buffer.buf);
    TEST_ASSERT_EQUAL_INT(0, receiver.int_flank);
    TEST_ASSERT_EQUAL_INT(0, receiver.int_timestamp_us);
}

static void test_1Byte_transmission(void)
{
    uint8_t expected_byte = 0b10100101; // 165 decimal
    ir_transmitter_send(&transmitter, &expected_byte, 1);
    wait_for_bytes(1, 10); // 1 Byte 10ms timout

    uint8_t actual_byte = tsrb_get_one(&receiver.recv_buffer);
    TEST_ASSERT_EQUAL_INT(expected_byte, actual_byte);
}

static void test_multiple_bytes_transmission(void)
{
    uint8_t expected_bytes[] = "Hallo Welt";
    uint8_t actual_bytes[sizeof(expected_bytes)] = {0};

    ir_transmitter_send(&transmitter, expected_bytes, sizeof(expected_bytes));
    wait_for_bytes(sizeof(expected_bytes), 10); // 11 Byte 10ms timout

    tsrb_get(&receiver.recv_buffer, actual_bytes, sizeof(expected_bytes));
    TEST_ASSERT_EQUAL_STRING((char*)expected_bytes, (char*)actual_bytes);
}

static Test *tests_ir_transiver_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_init_receiver),
        new_TestFixture(test_1Byte_transmission),
        new_TestFixture(test_multiple_bytes_transmission),
    };

    EMB_UNIT_TESTCALLER(ir_transiver_tests, set_up, NULL, fixtures);

    return (Test *)&ir_transiver_tests;
}

void tests_ir_transiver(void)
{
    TESTS_RUN(tests_ir_transiver_tests());
}

/** @} */
