/*
 * SPDX-License-Identifier: LGPL-2.1-only
 */

/**
 * @addtogroup  unittests
 * @{
 *
 * @file
 * @brief       Unittests for infrared module - main test suite
 */

#include "tests-infrared.h"

void tests_infrared(void)
{
    tests_ir_fsm();
    tests_ir_transiver();
}

/** @} */
