/*
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#pragma once

/**
 * @addtogroup  unittests
 * @{
 *
 * @file
 * @brief       Unittests for NEC protocol FSM
 */

#include "embUnit.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Entry point of the test suite
 */
void tests_fsm(void);
void tests_ir_receiver(void);
void tests_ir_transmitter(void);
void tests_infrared(void);

#ifdef __cplusplus
}
#endif

/** @} */
