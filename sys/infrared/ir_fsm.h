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
    ztimer_clock_t *clock_ms;
    ztimer_t timer;
    uint8_t current_byte;
    uint8_t current_bit;
    uint32_t droped_bytes;
    tsrb_t *recv_buffer;
    const ir_transmission_timing_t *timing;
} ir_fsm_state_t;


/* Function declarations */
int ir_fsm_handle_event(ir_fsm_state_t *fsm_state, Event event, uint32_t duration_us);
int ir_fsm_init(ir_fsm_state_t *self, tsrb_t *recv_buffer, const ir_transmission_timing_t *timing, ztimer_clock_t* clock_ms);

#endif