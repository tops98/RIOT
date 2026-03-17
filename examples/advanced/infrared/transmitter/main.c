#include "log.h"
#include "board.h"
#include "periph/pwm.h"
#include "ztimer.h"
#include "nec_protocol.h"



// ++++++++++++++++++++++++++ pwm config ++++++++++++++++++++++++++ //
#define PWM_FREQ 38000  // 38kHz
#define PWM_RES  100    // pwm resolution
#define PWM_ON 100      // 100% duty cycle
#define PWM_OFF 80      // 80% duty cycle

#define SET_PWM_SIGNAL(duty_cycle) pwm_set(PWM_DEV(0), 0, duty_cycle) 

nec_protocol_context_t nec_ctx; // NEC protocol context



// ++++++++++++++++++++++++++ functions for infrared ++++++++++++++++++++++++++ //

/**
 * @brief   Send a pulse of the given duration to the infrared device.
 *
 * @param   duration_us The duration of the pulse in microseconds.
 */
void send_pulse(uint32_t duration_us)
{
    SET_PWM_SIGNAL(PWM_ON);
    ztimer_sleep(ZTIMER_USEC, duration_us);
    SET_PWM_SIGNAL(PWM_OFF);
}

/**
 * @brief   Initialize the infrared device.
 *
 * This function initializes the infrared device by setting up the pwm pin
 * and the interrupt. It also initializes the nec protocol and sets up a
 * receive thread to handle interrupts from the infrared device.
 *
 * @return  true if the initialization was successful, false otherwise.
 */
bool init_infrared(void)
{
    LOG_INFO("Initializing infrared device...\n");
    
    // setup pwm pin
    uint32_t pwm_freq = pwm_init(PWM_DEV(0), PWM_LEFT, PWM_FREQ, PWM_RES);
    if(pwm_freq == 0){
        LOG_ERROR("Initializing pwm device failed!\n");
        return false;
    }
    LOG_INFO("Initialized pwm device with %lu Hz\n", pwm_freq);
    SET_PWM_SIGNAL(PWM_OFF);
    
    // nec protocol init
    nec_protocol_init(&nec_ctx, send_pulse);
    LOG_INFO("After init, nec_ctx.send_pulse: %p\n", (void*)nec_ctx.send_pulse);
    
    LOG_INFO("Infrared initialized!\n");
    return true;
}

/**
 * @brief   Main function of the sender example.
 *
 * This function initializes the infrared device and then sends a message
 * "Hello World from sender!" every 2 seconds using the NEC protocol.
 *
 * @return  int Always 0.
 */
int main(void)
{
    ztimer_sleep(ZTIMER_MSEC, 1000);
    LOG_INFO("\n\n\nStarting system...\n");
    
    if(!init_infrared()){
        LOG_ERROR("Initializing infrared device failed!\n");
        while (1){}
    }

    uint8_t message[] ="Hello World from sender!";
    while (true)
    {   
        nec_protocol_send(message, sizeof(message), &nec_ctx);
        LOG_INFO("nec_protocol_send executed\n");
        ztimer_sleep(ZTIMER_MSEC,2000);
    }
}