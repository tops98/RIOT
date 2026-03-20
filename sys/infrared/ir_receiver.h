#ifndef IR_RECEIVER_H
#define IR_RECEIVER_H

#include <stdint.h>
#include <periph/gpio.h>
#include <thread.h>
#include <sema.h>
#include "tsrb.h"
#include "ir_fsm.h"


typedef struct InfraredReceiver{
    bool int_flank;
    ztimer_now_t int_timestamp_us;
    gpio_t rec_pin;
    sema_t fsm_sema;
    ir_fsm_state_t fsm;
    tsrb_t recv_buffer;
    char receive_thread_stack[THREAD_STACKSIZE_DEFAULT];
    kernel_pid_t thread_pid;
} ir_receiver_t;

void ir_receiver_init_custom_timing(ir_receiver_t* receiver, gpio_t recv_gpio, uint8_t* in_buffer, uint32_t buffer_size, ir_transmission_timing_t timing);
void ir_receiver_init(ir_receiver_t* receiver, gpio_t recv_gpio, uint8_t* in_buffer, uint32_t buffer_size);
tsrb_t* ir_receiver_get_buffer(ir_receiver_t* ir_receiver);

#endif
