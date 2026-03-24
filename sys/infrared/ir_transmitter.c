#include "ir_transmitter.h"
#include "periph/pwm.h"
#include "ztimer.h"


static const ir_pwm_config_t defaul_conf = {
    .freqency = 38000,      // 38kHz,
    .resolution = 100,      // pwm resolution
    .send_duty_cycle = 100, // 100% duty cycle
    .off_duty_cycle = 80    // 80% duty cycle
};

const ir_pwm_config_t *IR_DEFAULT_PWM_CONF = &defaul_conf;

__attribute__((weak))
void ir_transmitter_sleep(ztimer_clock_t *clock, ztimer_now_t timeout)
{
    ztimer_sleep(clock, timeout);
}

static void send_pulse(const ir_transmitter_t *self, uint32_t duration_us)
{
    pwm_set(PWM_DEV(self->pwm_channel), 0, self->pwm_config->send_duty_cycle);
    ir_transmitter_sleep(self->clock_us, duration_us);
    pwm_set(PWM_DEV(self->pwm_channel), 0, self->pwm_config->off_duty_cycle);
}

void ir_transmitter_send(const ir_transmitter_t *transmitter, uint8_t* data, uint16_t len){
    uint8_t current_byte = 0;
    uint8_t current_bit = 0;

    // send start signal
    send_pulse(transmitter, transmitter->timing->start_high_time_us);
    ir_transmitter_sleep(transmitter->clock_us, transmitter->timing->start_low_time_us);

    for (uint8_t i = 0; i < len; i++)
    {
        current_byte = data[i];
        for (int8_t j = 7; j >= 0; j--)
        {
            current_bit = (current_byte >> j) & 1;
            send_pulse(transmitter, transmitter->timing->recv_high_time_us);
            ir_transmitter_sleep(transmitter->clock_us, (current_bit)? transmitter->timing->one_low_time_us: transmitter->timing->zero_low_time_us);
        }
    }
    send_pulse(transmitter, transmitter->timing->recv_high_time_us);
    ir_transmitter_sleep(transmitter->clock_us, transmitter->timing->transmission_timeout_ms);
}

void ir_transmitter_init(ir_transmitter_t *transmitter, uint8_t pwm_channel){
    transmitter->pwm_channel = pwm_channel;
    transmitter->clock_us = ZTIMER_USEC;
    transmitter->timing = IR_DEFAULT_TIMING;
    transmitter->pwm_config = IR_DEFAULT_PWM_CONF;
}
