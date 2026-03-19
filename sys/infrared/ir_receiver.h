#ifndef IR_RECEIVER_H
#define IR_RECEIVER_H

#include <stdint.h>
#include <periph/gpio.h>
#include <thread.h>
#include "tsrb.h"
#include "fsm.h"


typedef struct InfraredReceiver{
    bool int_flank;
    ztimer_now_t int_timestamp_us;
    gpio_t rec_pin;
    sema_t fsm_sema;
    fsm_state_t fsm;
    tsrb_t recv_buffer;
    char receive_thread_stack[THREAD_STACKSIZE_DEFAULT];
} ir_receiver_t;

const static ir_transmission_timing_t DEFAULT_TIMING = {
    .int_debouncing_time_us = 10,
    .timing_tollerance_us = 400,
    .transmission_timeout_ms = 2,
    .start_high_time_us = 9000,
    .start_low_time_us = 4500,
    .recv_high_time_us = 560,
    .zero_low_time_us = 560,
    .one_low_time_us = 1687
};

void ir_receiver_init_custom_timing(ir_receiver_t* receiver, gpio_t recv_gpio, uint8_t* in_buffer, uint32_t buffer_size, ir_transmission_timing_t timing);
void ir_receiver_init(ir_receiver_t* receiver, gpio_t recv_gpio, uint8_t* in_buffer, uint32_t buffer_size);
tsrb_t* ir_receiver_get_buffer(const ir_receiver_t* ir_receiver);

#endif
