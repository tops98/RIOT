#ifndef IR_TRANSMITTER_H
#define IR_TRANSMITTER_H

#include <stdint.h>
#include "ztimer.h"
#include "ir_receiver.h"


typedef struct Pwm_Config{
    uint32_t freqency;
    uint32_t resolution;
    uint32_t send_duty_cycle;
    uint32_t off_duty_cycle;
}ir_pwm_config_t;

typedef struct IR_Transmitter{
    uint8_t pwm_channel;
    ztimer_clock_t* clock_us;
    const ir_pwm_config_t* pwm_config;
    const ir_transmission_timing_t* timing;

}ir_transmitter_t;

extern const ir_pwm_config_t *IR_DEFAULT_PWM_CONF;

int ir_transmitter_init(ir_transmitter_t *transmitter, uint8_t pwm_channel);
int ir_transmitter_send(const ir_transmitter_t *tansmitter, uint8_t* data, uint16_t len);

#endif
