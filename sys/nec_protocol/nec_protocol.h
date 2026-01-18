#ifndef NEC_PROTOCOL_H
#define NEC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ztimer.h>
#include "message_buffer.h"

#define TIMING_ACCURCY_US 400
#define RECV_TIMEOUT_MS   2
#define START_HIGH_TIME_US ((uint32_t)9000)
#define START_LOW_TIME_US  ((uint32_t)4500)
#define RECV_HIGH_TIME_US  ((uint32_t)560)
#define ZERO_LOW_TIME_US  ((uint32_t)560)
#define ONE_LOW_TIME_US   ((uint32_t)1687)


typedef enum {
    STATE_IDLE,
    STATE_START,
    STATE_RECEIVE
} State;

typedef enum {
    EVENT_RISING,
    EVENT_FALLING,
    EVENT_TIMEOUT
} Event;

typedef void (*SendPulseFn)(uint32_t pulse_durration_us);

typedef struct {
    MessageQueue msg_buffer;
    Message* current_msg;
    uint32_t bits_received;
    State current_state;
    ztimer_t timer;
    SendPulseFn send_pulse;
} nec_protocol_context_t;

typedef bool (*TimeGuardFn)(uint32_t duration_us, uint32_t expected_duration_us);
typedef void (*ActionFn)(nec_protocol_context_t *ctx);


typedef struct {
    State       from;
    Event       event;
    TimeGuardFn guard;
    uint32_t    expected_duration_us;
    ActionFn    action;
    State       to;
} Transition;

/* Function declarations */
bool check_timing(uint32_t duration_us, uint32_t expected_duration_us);
void nec_protocol_init(nec_protocol_context_t *ctx, SendPulseFn send_pulse);
void nec_protocol_handle_event(Event event, uint32_t duration_us, nec_protocol_context_t *ctx);
void nec_protocoll_send(uint8_t* data, uint16_t len, nec_protocol_context_t *ctx);
#endif /* NEC_PROTOCOL_H */