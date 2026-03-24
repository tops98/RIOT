#include "ir_fsm.h"
#include <stddef.h>
#include <ztimer.h>
#include "debug.h"



typedef enum {
    START_HIGH_TIME_US,
    START_LOW_TIME_US,
    RECV_HIGH_TIME_US,
    ZERO_LOW_TIME_US,
    ONE_LOW_TIME_US,    
} Timings;

typedef void (*SendPulseFn)(uint32_t pulse_durration_us);
typedef bool (*TimeGuardFn)(ir_fsm_state_t *self, uint32_t duration_us, Timings expected_duration_us);
typedef void (*ActionFn)(ir_fsm_state_t *self);

typedef struct Transition{
    State       from;
    Event       event;
    TimeGuardFn guard;
    uint32_t    expected_duration_us;
    ActionFn    action;
    State       to;
} transition_t;



static void timer_callback(void *arg)
{
    ir_fsm_handle_event(arg, EVENT_TIMEOUT, 0);
}

static void arm_timer(ir_fsm_state_t *self){
    self->timer.arg = self;
    self->timer.callback = timer_callback;
    ztimer_set(self->clock_ms, &self->timer, self->timing->transmission_timeout_ms);
}

static void reset_timer(ir_fsm_state_t *self){
    ztimer_remove(self->clock_ms, &self->timer);
    arm_timer(self);
}

static void bit_received(ir_fsm_state_t *self, bool bit)
{
    reset_timer(self);
    int status = 0;
    self->current_byte |= bit << (7 - self->current_bit++);
    if(self->current_bit >=8){
        if(tsrb_full(self->recv_buffer) == 0){
            status = tsrb_add_one(self->recv_buffer, self->current_byte);
        }
        self->current_byte = self->current_bit = 0;
    }
}

static bool check_timing(ir_fsm_state_t *self,uint32_t duration_us, Timings timing)
{
    uint32_t expected_duration_us = 0;

    switch(timing){
        case START_HIGH_TIME_US:
            expected_duration_us = self->timing->start_high_time_us;        
            break;
        case START_LOW_TIME_US:
            expected_duration_us = self->timing->start_low_time_us;
            break;
        case RECV_HIGH_TIME_US:
            expected_duration_us = self->timing->recv_high_time_us;
            break;
        case ZERO_LOW_TIME_US:
            expected_duration_us = self->timing->zero_low_time_us;
            break;
        case ONE_LOW_TIME_US:
            expected_duration_us = self->timing->one_low_time_us;
            break;   
    }

    uint32_t diff = (duration_us > expected_duration_us) ? 
                    (duration_us - expected_duration_us) : 
                    (expected_duration_us - duration_us);
    return diff <= self->timing->timing_tollerance_us;
}

static void receive_logic_0(ir_fsm_state_t *self)
{
    bit_received(self, false);
}

static void receive_logic_1(ir_fsm_state_t *self)
{
    bit_received(self, true);
}

static void reset_byte_buffer(ir_fsm_state_t *self){
    self->current_byte = self->current_bit = 0;
}


/*
 * WARNING:
 * If there are multiple transitions for the same STATE/EVENT pair,
 * the first matching transition will be used.
 */
static const transition_t fsm[] = {
    /* IDLE STATE */
    { STATE_IDLE,    EVENT_FALLING, NULL,         0,                    NULL,               STATE_IDLE },
    { STATE_IDLE,    EVENT_RISING,  NULL,         0,                    arm_timer,          STATE_START },

    /* START STATE */
    { STATE_START,   EVENT_FALLING, check_timing, START_HIGH_TIME_US,   reset_timer,        STATE_START },
    { STATE_START,   EVENT_FALLING, NULL,         0,                    NULL,               STATE_IDLE },

    { STATE_START,   EVENT_RISING, check_timing, START_LOW_TIME_US,     reset_byte_buffer,  STATE_RECEIVE },
    { STATE_START,   EVENT_RISING,  NULL,         0,                    NULL,               STATE_IDLE },
    
    { STATE_START,   EVENT_TIMEOUT, NULL,         0,                    NULL,               STATE_IDLE },

    /* RECEIVE STATE */
    { STATE_RECEIVE, EVENT_FALLING, check_timing, RECV_HIGH_TIME_US,    reset_timer,        STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_FALLING, NULL,         0,                    NULL,               STATE_IDLE },

    { STATE_RECEIVE, EVENT_RISING,  check_timing, ZERO_LOW_TIME_US,     receive_logic_0,    STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_RISING,  check_timing, ONE_LOW_TIME_US,      receive_logic_1,    STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_RISING,  NULL,         0,                    NULL,               STATE_IDLE },
    
    { STATE_RECEIVE, EVENT_TIMEOUT, NULL,         0,                    NULL,               STATE_IDLE },
};
static const uint8_t num_transitions = sizeof(fsm) / sizeof(transition_t);

void ir_fsm_handle_event(ir_fsm_state_t *fsm_state, Event event, uint32_t duration_us)
{
    transition_t current_transition = fsm[0];
    for (size_t i = 0; i <num_transitions; i++){
        current_transition= fsm[i];
        // check if event and state are matching the transition
        if (current_transition.from != fsm_state->current_state || current_transition.event != event){
            continue;
        }
        // is guard failing
        if (current_transition.guard != NULL && !current_transition.guard(fsm_state, duration_us, current_transition.expected_duration_us)){
            continue;
        }

        // execute action if available
        if (current_transition.action != NULL){
            current_transition.action(fsm_state);
        }
        // DEBUG_PRINT("\n\rSWITCHING FROM [%d] -> [%d] with event [%d] durartion = [%d]\n", self->current_state, current_transition.to, event, duration_us);
        // switch state
        fsm_state->current_state = current_transition.to;
        break;
    }
}

ir_fsm_state_t ir_fsm_create(tsrb_t *recv_buffer, const ir_transmission_timing_t* timing, ztimer_clock_t* clock_ms){
    
    ir_fsm_state_t fsm = {
        .timing = timing,
        .clock_ms = clock_ms,
        .recv_buffer = recv_buffer,
        .current_bit = 0,
        .current_byte = 0,
        .current_state = STATE_IDLE,
    };

    ztimer_remove(fsm.clock_ms, &fsm.timer);

    return fsm;
}
