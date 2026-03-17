/*
 * @file periph_conf.h
 *
 * Extends the periph_conf.h file with configuration for pwm used.
 * Configurations only exposes the pin D12 on arduino-feather-nrf52840-sense board
 * 
 */
#include_next "periph_conf.h"
#ifndef PERIPH_CONF_H
#define PERIPH_CONF_H

#include "periph_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

// Define PWM device 0
static const pwm_conf_t pwm_config[] = {
    {
        .dev      = NRF_PWM0,              // Use hardware PWM0
        .pin      = { 
                      GPIO_PIN(0, 6),      // Channel 0 (Feather Pin D11)
                    },
    }
};

#define PWM_NUMOF           ARRAY_SIZE(pwm_config)

#ifdef __cplusplus
}
#endif

#endif /* PERIPH_CONF_H */