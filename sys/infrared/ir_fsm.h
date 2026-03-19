#ifndef IR_FSM_H
#define IR_FSM_H


#include <stdint.h>
#include <stdbool.h>
#include "tsrb.h"
#include "ztimer.h"


typedef enum {
    STATE_IDLE,
    STATE_START,
    STATE_RECEIVE
} State;

typedef enum {
    EVENT_FALLING,
    EVENT_RISING,
    EVENT_TIMEOUT
} Event;

typedef struct IrTransmissionTiming {
    uint16_t int_debouncing_time_us;
    uint16_t timing_tollerance_us;
    uint16_t transmission_timeout_ms;
    uint16_t start_high_time_us;
    uint16_t start_low_time_us;
    uint16_t recv_high_time_us;
    uint16_t zero_low_time_us;
    uint16_t one_low_time_us; 
} ir_transmission_timing_t;

typedef struct FsmState{
    State current_state;
    ztimer_t timer;
    uint8_t current_byte;
    uint8_t current_bit;
    tsrb_t *recv_buffer;
    ir_transmission_timing_t timing;
} ir_fsm_state_t;

static const ir_transmission_timing_t IR_DEFAULT_TIMING = {
    .int_debouncing_time_us = 10,
    .timing_tollerance_us = 400,
    .transmission_timeout_ms = 2,
    .start_high_time_us = 9000,
    .start_low_time_us = 4500,
    .recv_high_time_us = 560,
    .zero_low_time_us = 560,
    .one_low_time_us = 1687
};

/* Function declarations */
void ir_fsm_handle_event(Event event, uint32_t duration_us, ir_fsm_state_t *ctx);
ir_fsm_state_t ir_fsm_create(tsrb_t *recv_buffer, ir_transmission_timing_t timing);

#endif