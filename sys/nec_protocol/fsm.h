#ifndef NEC_PROTOCOL_H
#define NEC_PROTOCOL_H


#include <stdint.h>
#include <stdbool.h>
#include "tsrb.h"


typedef void (*SendPulseFn)(uint32_t pulse_durration_us);
typedef bool (*TimeGuardFn)(uint32_t duration_us, uint32_t expected_duration_us);
typedef void (*ActionFn)(fsm_state_t *ctx);


typedef struct Transition transition_t;
typedef struct FsmState fsm_state_t;

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

/* Function declarations */
void fsm_handle_event(Event event, uint32_t duration_us, fsm_state_t *ctx);
fsm_state_t fsm_init(tsrb_t *recv_buffer, ir_transmission_timing_t timing);

#endif /* NEC_PROTOCOL_H */