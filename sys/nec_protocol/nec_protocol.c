#include "nec_protocol.h"
#include "ztimer.h"
#include "shell.h"

bool check_timing(uint32_t duration_us, uint32_t expected_duration_us)
{
    return (duration_us - expected_duration_us) < TIMING_ACCURCY_US;
}

static void start_receival(nec_protocol_context_t *ctx)
{
    ctx->current_msg = message_queue_get_editable(&ctx->msg_buffer);
    ctx->current_msg->len = 0;
    ctx->bits_received = 0;
}

static void end_receival(nec_protocol_context_t *ctx)
{
    ctx->current_msg->len = ctx->bits_received / 8;
    message_queue_commit(&ctx->msg_buffer);
}

void timer_callback(void *arg)
{
    nec_protocol_context_t *ctx = (nec_protocol_context_t*)arg;
    end_receival(ctx);
    ctx->current_state = STATE_IDLE;
}

static void set_timout(nec_protocol_context_t *ctx){
    ctx->timer.arg = ctx;
    ztimer_set(ZTIMER_MSEC, &ctx->timer, RECV_TIMEOUT_MS);
}

static void bit_received(nec_protocol_context_t *ctx, bool bit)
{
    ztimer_remove(ZTIMER_MSEC, &ctx->timer);
    ctx->current_msg->data[ctx->bits_received / 8] |=
        (bit << (7 - (ctx->bits_received % 8)));
    ctx->bits_received++;
}

static void receive_logic_0(nec_protocol_context_t *ctx)
{
    bit_received(ctx, false);
}

static void receive_logic_1(nec_protocol_context_t *ctx)
{
    bit_received(ctx, true);
}

/*
 * WARNING:
 * If there are multiple transitions for the same STATE/EVENT pair,
 * the first matching transition will be used.
 */
const Transition fsm[] = {
    /* IDLE STATE */
    { STATE_IDLE,    EVENT_FALLING, NULL,         0,                    NULL,            STATE_IDLE },
    { STATE_IDLE,    EVENT_RISING,  NULL,         0,                    NULL,            STATE_START },

    /* START STATE */
    { STATE_START,   EVENT_FALLING, check_timing, START_HIGH_TIME_US,   NULL,            STATE_START },
    { STATE_START,   EVENT_FALLING, NULL,         0,                    NULL,            STATE_IDLE },

    { STATE_START,   EVENT_RISING, check_timing, START_LOW_TIME_US,    start_receival,  STATE_RECEIVE },
    { STATE_START,   EVENT_RISING,  NULL,         0,                    NULL,            STATE_IDLE },

    /* RECEIVE STATE */
    { STATE_RECEIVE, EVENT_FALLING, check_timing, RECV_HIGH_TIME_US,    set_timout,      STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_FALLING, NULL,         0,                    NULL,            STATE_IDLE },

    { STATE_RECEIVE, EVENT_RISING,  check_timing, ZERO_LOW_TIME_US,     receive_logic_0, STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_RISING,  check_timing, ONE_LOW_TIME_US,      receive_logic_1, STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_RISING,  NULL,         0,                    end_receival,    STATE_IDLE },
};

void handle_event(Event event, uint32_t duration_us, nec_protocol_context_t *ctx)
{
    Transition current_transition = fsm[0];
    for (size_t i = 0; i < TRANSITION_COUNT; i++){
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
        // switch state
        ctx->current_state = current_transition.to;
        break;
    }
}

void nec_protocol_init(nec_protocol_context_t *ctx){
    ctx->current_state = STATE_IDLE;
    ctx->bits_received = 0;
    ctx->msg_buffer.head = 0;
    ctx->msg_buffer.tail = 0;
    ctx->timer.callback = timer_callback;
    ctx->timer.arg = NULL;
}
