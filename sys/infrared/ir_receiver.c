#include "ir_receiver.h"

#include <errno.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory.h>
#include "ztimer.h"
#include "log.h"


static const ir_transmission_timing_t default_timing = {
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
    ir_receiver_t* self = (ir_receiver_t*)arg;
    self->int_flank = gpio_read(self->rec_pin);
    self->int_timestamp_us = ztimer_now(ZTIMER_USEC);
    sema_post(&self->fsm_sema);
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
        ir_fsm_handle_event(&ctx->fsm,ctx->int_flank, duration);
    }
}

int ir_receiver_init(ir_receiver_t* receiver, gpio_t recv_gpio, uint8_t* in_buffer, uint32_t buffer_size){
    int status = 0;

    /* No nullpointer and buffer_size must be a power of two */
    if(receiver == NULL || in_buffer == NULL || (buffer_size & (buffer_size-1))){
        LOG_ERROR("[ir_receiver_init] NULL pointer\n");
        return -EINVAL;
    }
    
    memset(receiver, 0, sizeof(ir_receiver_t));
    receiver->rec_pin = recv_gpio;
    sema_create(&receiver->fsm_sema, 0);
    tsrb_init(&receiver->recv_buffer, in_buffer, buffer_size);
    tsrb_clear(&receiver->recv_buffer);
    gpio_init_int(recv_gpio, GPIO_IN_PU, GPIO_BOTH, interrupt_callback, receiver);
    
    status = ir_fsm_init(&receiver->fsm ,&receiver->recv_buffer, IR_DEFAULT_TIMING, ZTIMER_MSEC);
    if(status < 0){
        LOG_ERROR("[ir_receiver_init] Could not create fsm\n");
        return status;
    }

    receiver->thread_pid = thread_create(receiver->receive_thread_stack, sizeof(receiver->receive_thread_stack), THREAD_PRIORITY_MAIN - 1, 0, receive_thread, receiver,"ir_recv");
    if(receiver->thread_pid < 0){
        LOG_ERROR("[ir_receiver_init] Could not create receive thread\n");
        return receiver->thread_pid;
    }

    return 0;
}

tsrb_t* ir_receiver_get_buffer(ir_receiver_t* receiver){
    return &receiver->recv_buffer;
}