#include "ir_fsm.h"

#include <stddef.h>
#include <errno.h>
#include "ztimer.h"
#include "log.h"



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
    self->current_byte |= bit << (7 - self->current_bit++);
    if(self->current_bit >=8){
        if(tsrb_full(self->recv_buffer) == 0){
            tsrb_add_one(self->recv_buffer, self->current_byte);
        }else{
            self->droped_bytes++;
            LOG_WARNING("[ir_fsm] droped %d bytes\n", self->current_byte);
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

int ir_fsm_handle_event(ir_fsm_state_t *fsm_state, Event event, uint32_t duration_us)
{
    bool found = false;
    if(fsm_state == NULL){
        LOG_ERROR("[ir_fsm_handle_event] fsm_state is NULL\n");
        return -EINVAL;
    }

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
        // switch state
        fsm_state->current_state = current_transition.to;
        found = true;
        break;
    }

    if(!found){
        LOG_ERROR("[ir_fsm_handle_event] no matching transition found\n");
        return -ENOTSUP;
    }

    return 0;
}

int ir_fsm_init(ir_fsm_state_t *self, tsrb_t *recv_buffer, const ir_transmission_timing_t* timing, ztimer_clock_t* clock_ms){
    
    if(self == NULL || recv_buffer == NULL || timing == NULL || clock_ms == NULL){
        LOG_ERROR("[ir_fsm_init] NULL pointer\n");
        return -EINVAL;
    }
    
    self->timing = timing;
    self->clock_ms = clock_ms;
    self->recv_buffer = recv_buffer;
    self->current_bit = 0;
    self->current_byte = 0;
    self->current_state = STATE_IDLE;

    ztimer_remove(self->clock_ms, &self->timer);

    return 0;
}
