#include "ir_transmitter.h"
#include "periph/pwm.h"
#include "ztimer.h"


#define PWM_FREQ 38000  // 38kHz
#define PWM_RES  100    // pwm resolution
#define PWM_ON 100      // 100% duty cycle
#define PWM_OFF 80      // 80% duty cycle

#define SET_PWM_SIGNAL(duty_cycle) pwm_set(PWM_DEV(0), 0, duty_cycle)


void send_pulse(uint32_t duration_us)
{
    SET_PWM_SIGNAL(PWM_ON);
    ztimer_sleep(ZTIMER_USEC, duration_us);
    SET_PWM_SIGNAL(PWM_OFF);
}

void ir_transmitter_send(uint8_t* data, uint16_t len){
    ir_transmitter_send_custom_timing(data, len, IR_DEFAULT_TIMING);
}

void ir_transmitter_send_custom_timing(uint8_t* data, uint16_t len, ir_transmission_timing_t timing){
    uint8_t current_byte = 0;
    uint8_t current_bit = 0;

    // send start signal
    send_pulse(timing.start_high_time_us);
    ztimer_sleep(ZTIMER_USEC, timing.start_low_time_us);

    for (uint8_t i = 0; i < len; i++)
    {
        current_byte = data[i];
        for (int8_t j = 7; j >= 0; j--)
        {
            current_bit = (current_byte >> j) & 1;
            send_pulse(timing.recv_high_time_us);
            ztimer_sleep(ZTIMER_USEC, (current_bit)? timing.one_low_time_us: timing.zero_low_time_us);
        }
    }
    send_pulse(timing.recv_high_time_us);
    ztimer_sleep(ZTIMER_MSEC, timing.transmission_timeout_ms);
}
