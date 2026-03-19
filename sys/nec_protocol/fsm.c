#include "fsm.h"
#include <stddef.h>
#include <ztimer.h>
#include "debug.h"


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

typedef enum {
    START_HIGH_TIME_US,
    START_LOW_TIME_US,
    RECV_HIGH_TIME_US,
    ZERO_LOW_TIME_US,
    ONE_LOW_TIME_US,    
} Timings;

typedef struct Transition{
    State       from;
    Event       event;
    TimeGuardFn guard;
    uint32_t    expected_duration_us;
    ActionFn    action;
    State       to;
} transition_t;

typedef struct FsmState{
    State current_state;
    ztimer_t timer;
    uint8_t current_byte;
    uint8_t current_bit;
    tsrb_t *recv_buffer;
    ir_transmission_timing_t timing;
} fsm_state_t;


static void bit_received(fsm_state_t *ctx, bool bit)
{
    ztimer_remove(ZTIMER_MSEC, &ctx->timer);

    ctx->current_byte |= bit << (7 - ctx->current_bit++);
    if(ctx->current_bit >=8){
        if(tsrb_full(&ctx->recv_buffer) == 0){
            tsrb_add_one(&ctx->recv_buffer, ctx->current_byte);
        }
        ctx->current_byte = ctx->current_bit = 0;
    }
}

bool check_timing(uint32_t duration_us, uint8_t timing, fsm_state_t *ctx)
{
    uint32_t expected_duration_us = 0;

    switch(timing){
        case START_HIGH_TIME_US:
            expected_duration_us = ctx->timing.start_high_time_us;        
            break;
        case START_LOW_TIME_US:
            expected_duration_us = ctx->timing.start_low_time_us;
            break;
        case RECV_HIGH_TIME_US:
            expected_duration_us = ctx->timing.recv_high_time_us;
            break;
        case ZERO_LOW_TIME_US:
            expected_duration_us = ctx->timing.zero_low_time_us;
            break;
        case ONE_LOW_TIME_US:
            expected_duration_us = ctx->timing.one_low_time_us;
            break;   
    }

    uint32_t diff = (duration_us > expected_duration_us) ? 
                    (duration_us - expected_duration_us) : 
                    (expected_duration_us - duration_us);
    return diff < ctx->timing.timing_tollerance_us;
}

static void receive_logic_0(fsm_state_t *ctx)
{
    bit_received(ctx, false);
}

static void receive_logic_1(fsm_state_t *ctx)
{
    bit_received(ctx, true);
}

void reset_byte_buffer(fsm_state_t *ctx){
    ctx->current_byte = ctx->current_bit = 0;
}

void timer_callback(void *arg)
{
    nec_protocol_handle_event(EVENT_TIMEOUT, 0, arg);
}

static void set_timout(fsm_state_t *ctx){
    ctx->timer.arg = ctx;
    ztimer_set(ZTIMER_MSEC, &ctx->timer, ctx->timing.transmission_timeout_ms);
}


/*
 * WARNING:
 * If there are multiple transitions for the same STATE/EVENT pair,
 * the first matching transition will be used.
 */
const transition_t fsm[] = {
    /* IDLE STATE */
    { STATE_IDLE,    EVENT_FALLING, NULL,         0,                    NULL,               STATE_IDLE },
    { STATE_IDLE,    EVENT_RISING,  NULL,         0,                    NULL,               STATE_START },

    /* START STATE */
    { STATE_START,   EVENT_FALLING, check_timing, START_HIGH_TIME_US,   NULL,               STATE_START },
    { STATE_START,   EVENT_FALLING, NULL,         0,                    NULL,               STATE_IDLE },

    { STATE_START,   EVENT_RISING, check_timing, START_LOW_TIME_US,     reset_byte_buffer,  STATE_RECEIVE },
    { STATE_START,   EVENT_RISING,  NULL,         0,                    NULL,               STATE_IDLE },

    /* RECEIVE STATE */
    { STATE_RECEIVE, EVENT_FALLING, check_timing, RECV_HIGH_TIME_US,    set_timout,         STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_FALLING, NULL,         0,                    NULL,               STATE_IDLE },

    { STATE_RECEIVE, EVENT_RISING,  check_timing, ZERO_LOW_TIME_US,     receive_logic_0,    STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_RISING,  check_timing, ONE_LOW_TIME_US,      receive_logic_1,    STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_RISING,  NULL,         0,                    NULL,               STATE_IDLE },
    
    { STATE_RECEIVE, EVENT_TIMEOUT, NULL,         0,                    NULL,               STATE_IDLE },
};
static const uint8_t num_transitions = sizeof(fsm) / sizeof(transition_t);

void fsm_handle_event(Event event, uint32_t duration_us, fsm_state_t *ctx)
{

    transition_t current_transition = fsm[0];
    for (size_t i = 0; i <num_transitions; i++){
        current_transition= fsm[i];
        // check if event and state are matching the transition
        if (current_transition.from != ctx->current_state || current_transition.event != event){
            continue;
        }
        // is guard failing
        if (current_transition.guard != NULL && !current_transition.guard(duration_us, current_transition.expected_duration_us)){
            continue;
        }

        // execute action if available
        if (current_transition.action != NULL){
            current_transition.action(ctx);
        }
        // DEBUG_PRINT("\n\rSWITCHING FROM [%d] -> [%d] with event [%d] durartion = [%d]\n", ctx->current_state, current_transition.to, event, duration_us);
        // switch state
        ctx->current_state = current_transition.to;
        break;
    }
}

fsm_state_t fsm_init(tsrb_t *recv_buffer, ir_transmission_timing_t timing){
    
    fsm_state_t fsm = {
        .timing = timing,
        .recv_buffer = recv_buffer,
        .current_bit = 0,
        .current_byte = 0,
        .current_state = STATE_IDLE,
    };

    ztimer_remove(ZTIMER_MSEC, &fsm.timer);
    fsm.timer.callback = timer_callback;
    fsm.timer.arg = NULL;

    return fsm;
}
