#include "ir_receiver.h"
#include <stddef.h>
#include <stdbool.h>
#include <ztimer.h>
#include <memory.h>


static const ir_transmission_timing_t default_timing = {
    .int_debouncing_time_us = 10, // TODO: move to receiver struct

    .timing_tollerance_us = 400,
    .transmission_timeout_ms = 2,
    .start_high_time_us = 9000,
    .start_low_time_us = 4500,
    .recv_high_time_us = 560,
    .zero_low_time_us = 560,
    .one_low_time_us = 1687
};

const ir_transmission_timing_t* IR_DEFAULT_TIMING = &default_timing;

void interrupt_callback(void* arg)
{
    ir_receiver_t* ctx = (ir_receiver_t*)arg;
    ctx->int_flank = gpio_read(ctx->rec_pin);
    ctx->int_timestamp_us = ztimer_now(ZTIMER_USEC);
    sema_post(&ctx->fsm_sema);
}

void *receive_thread(void* arg)
{   
    ztimer_now_t rising_time = 0;
    ztimer_now_t falling_time = 0;
    ztimer_now_t duration = 0;
    ir_receiver_t* ctx = (ir_receiver_t*)arg;
    while (true)
    {
        sema_wait(&ctx->fsm_sema);
        if(!ctx->int_flank){
            falling_time = ctx->int_timestamp_us;
            duration = falling_time - rising_time;
        }else{
            rising_time = ctx->int_timestamp_us;
            duration = rising_time - falling_time;
        }
        ir_fsm_handle_event(ctx->int_flank, duration, &ctx->fsm);
    }
}

void ir_receiver_init(ir_receiver_t* receiver, gpio_t recv_gpio, uint8_t* in_buffer, uint32_t buffer_size){
    ir_receiver_init_custom_timing(receiver, recv_gpio, in_buffer, buffer_size, IR_DEFAULT_TIMING);
}

void ir_receiver_init_custom_timing(ir_receiver_t* receiver, gpio_t recv_gpio, uint8_t* in_buffer, uint32_t buffer_size, const ir_transmission_timing_t *timing){
    memset(receiver, 0, sizeof(ir_receiver_t));
    
    receiver->fsm = ir_fsm_create(&receiver->recv_buffer, timing, ZTIMER_MSEC);
    receiver->rec_pin = recv_gpio;

    sema_create(&receiver->fsm_sema, 0);

    tsrb_init(&receiver->recv_buffer, in_buffer, buffer_size);
    tsrb_clear(&receiver->recv_buffer);

    gpio_init_int(recv_gpio, GPIO_IN_PU, GPIO_BOTH, interrupt_callback, receiver);
    receiver->thread_pid = thread_create(receiver->receive_thread_stack, sizeof(receiver->receive_thread_stack), THREAD_PRIORITY_MAIN - 1, 0, receive_thread, receiver,"ir_recv");
}

tsrb_t* ir_receiver_get_buffer(ir_receiver_t* receiver){
    return &receiver->recv_buffer;
}