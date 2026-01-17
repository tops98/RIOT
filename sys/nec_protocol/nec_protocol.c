#include "nec_protocol.h"

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

static void bit_received(nec_protocol_context_t *ctx, bool bit)
{
    ctx->current_msg->data[ctx->bits_received / 8] |=
        (bit << (ctx->bits_received % 8));
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

    { STATE_START,   EVENT_FALLING, check_timing, START_LOW_TIME_US,    start_receival,  STATE_RECEIVE },
    { STATE_START,   EVENT_RISING,  NULL,         0,                    NULL,            STATE_IDLE },

    /* RECEIVE STATE */
    { STATE_RECEIVE, EVENT_FALLING, check_timing, RECV_HIGH_TIME_US,    NULL,            STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_FALLING, NULL,         0,                    NULL,            STATE_IDLE },

    { STATE_RECEIVE, EVENT_RISING,  check_timing, ZERO_LOW_TIME_US,     receive_logic_0, STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_RISING,  check_timing, ONE_LOW_TIME_US,      receive_logic_1, STATE_RECEIVE },
    { STATE_RECEIVE, EVENT_RISING,  NULL,         0,                    end_receival,   STATE_IDLE },
};

void handle_event(Event event, uint32_t duration_us, nec_protocol_context_t *ctx)
{
    for (size_t i = 0; i < TRANSITION_COUNT; i++){

        // check if event and state are matching the transition
        if (fsm[i].from != ctx->current_state || fsm[i].event != event){
            continue;
        }
        // is guard failing
        if (fsm[i].guard != NULL && !fsm[i].guard(duration_us, fsm[i].expected_duration_us)){
            continue;
        }

        // execute action if available
        if (fsm[i].action != NULL){
            fsm[i].action(ctx);
        }
        // switch state
        ctx->current_state = fsm[i].to;
    }
}
