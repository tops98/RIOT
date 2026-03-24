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
void tests_ir_fsm(void);
void tests_ir_transiver(void);
void tests_infrared(void);

#ifdef __cplusplus
}
#endif

/** @} */
