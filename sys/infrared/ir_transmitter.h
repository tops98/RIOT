#ifndef IR_TRANSMITTER_H
#define IR_TRANSMITTER_H

#include <stdint.h>
#include "ir_receiver.h"

void ir_transmitter_send_custom_timing(uint8_t* data, uint16_t len, ir_transmission_timing_t timing);
void ir_transmitter_send(uint8_t* data, uint16_t len);

#endif