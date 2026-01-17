/*
 * SPDX-License-Identifier: LGPL-2.1-only
 */

/**
 * @{
 *
 * @file
 */

#include <stdint.h>
#include <string.h>

#include "embUnit/embUnit.h"

#include "message_buffer.h"
#include "tests-message_buffer.h"

#define TEST_MESSAGE_LEN    (10U)
#define CANARY_BYTE         (0xABU)

static MessageQueue _msg_queue;

static void tear_down(void)
{
    memset(&_msg_queue, 0, sizeof(_msg_queue));
}

static void test_init_empty(void)
{
    /* Queue should be initialized as empty */
    TEST_ASSERT_EQUAL_INT(0, message_queue_length(&_msg_queue));
}

static void test_pop_empty(void)
{
    Message buffer;

    /* Popping from empty queue should return false */
    TEST_ASSERT_EQUAL_INT(0, message_queue_pop(&_msg_queue, &buffer));
}

static void test_single_message(void)
{
    Message *editable;
    Message buffer;
    uint32_t i;

    /* Get editable message */
    editable = message_queue_get_editable(&_msg_queue);
    TEST_ASSERT(NULL != editable);

    /* Fill with test data */
    editable->len = TEST_MESSAGE_LEN;
    for (i = 0; i < TEST_MESSAGE_LEN; i++) {
        editable->data[i] = i;
    }

    /* Commit message */
    message_queue_commit(&_msg_queue);
    TEST_ASSERT_EQUAL_INT(1, message_queue_length(&_msg_queue));

    /* Pop message */
    TEST_ASSERT_EQUAL_INT(1, message_queue_pop(&_msg_queue, &buffer));
    TEST_ASSERT_EQUAL_INT(TEST_MESSAGE_LEN, buffer.len);
    for (i = 0; i < TEST_MESSAGE_LEN; i++) {
        TEST_ASSERT_EQUAL_INT(i, buffer.data[i]);
    }

    /* Queue should now be empty */
    TEST_ASSERT_EQUAL_INT(0, message_queue_length(&_msg_queue));
}

static void test_multiple_messages(void)
{
    Message *editable;
    Message buffer;
    uint32_t i, j;

    /* Add multiple messages */
    for (j = 0; j < 3; j++) {
        editable = message_queue_get_editable(&_msg_queue);
        editable->len = TEST_MESSAGE_LEN;
        for (i = 0; i < TEST_MESSAGE_LEN; i++) {
            editable->data[i] = (j * 10) + i;
        }
        message_queue_commit(&_msg_queue);
    }

    TEST_ASSERT_EQUAL_INT(3, message_queue_length(&_msg_queue));

    /* Pop and verify all messages */
    for (j = 0; j < 3; j++) {
        TEST_ASSERT_EQUAL_INT(1, message_queue_pop(&_msg_queue, &buffer));
        TEST_ASSERT_EQUAL_INT(TEST_MESSAGE_LEN, buffer.len);
        for (i = 0; i < TEST_MESSAGE_LEN; i++) {
            TEST_ASSERT_EQUAL_INT((j * 10) + i, buffer.data[i]);
        }
    }

    /* Queue should be empty */
    TEST_ASSERT_EQUAL_INT(0, message_queue_length(&_msg_queue));
    TEST_ASSERT_EQUAL_INT(0, message_queue_pop(&_msg_queue, &buffer));
}

static void test_max_capacity(void)
{
    Message *editable;
    Message buffer;
    uint32_t i;

    /* Fill queue to capacity */
    for (i = 0; i < MSG_BUFFER_SIZE; i++) {
        editable = message_queue_get_editable(&_msg_queue);
        editable->len = 1;
        editable->data[0] = i;
        message_queue_commit(&_msg_queue);
    }

    TEST_ASSERT_EQUAL_INT(MSG_BUFFER_SIZE, message_queue_length(&_msg_queue));

    /* Verify all messages */
    for (i = 0; i < MSG_BUFFER_SIZE; i++) {
        TEST_ASSERT_EQUAL_INT(1, message_queue_pop(&_msg_queue, &buffer));
        TEST_ASSERT_EQUAL_INT(1, buffer.len);
        TEST_ASSERT_EQUAL_INT(i, buffer.data[0]);
    }

    TEST_ASSERT_EQUAL_INT(0, message_queue_length(&_msg_queue));
}

static void test_varying_lengths(void)
{
    Message *editable;
    Message buffer;
    uint32_t i, j;

    /* Add messages with varying lengths */
    for (i = 1; i <= 5; i++) {
        editable = message_queue_get_editable(&_msg_queue);
        editable->len = i;
        for (j = 0; j < i; j++) {
            editable->data[j] = j + i;
        }
        message_queue_commit(&_msg_queue);
    }

    TEST_ASSERT_EQUAL_INT(5, message_queue_length(&_msg_queue));

    /* Pop and verify messages have correct lengths */
    for (i = 1; i <= 5; i++) {
        TEST_ASSERT_EQUAL_INT(1, message_queue_pop(&_msg_queue, &buffer));
        TEST_ASSERT_EQUAL_INT(i, buffer.len);
        for (j = 0; j < i; j++) {
            TEST_ASSERT_EQUAL_INT(j + i, buffer.data[j]);
        }
    }
}

static void test_wrap_around(void)
{
    Message *editable;
    Message buffer;
    uint32_t i, j;

    /* Add and remove messages to cause wrap-around */
    for (j = 0; j < 2; j++) {
        /* Add MSG_BUFFER_SIZE messages */
        for (i = 0; i < MSG_BUFFER_SIZE; i++) {
            editable = message_queue_get_editable(&_msg_queue);
            editable->len = 1;
            editable->data[0] = (j * MSG_BUFFER_SIZE) + i;
            message_queue_commit(&_msg_queue);
        }

        /* Remove all messages */
        for (i = 0; i < MSG_BUFFER_SIZE; i++) {
            TEST_ASSERT_EQUAL_INT(1, message_queue_pop(&_msg_queue, &buffer));
            TEST_ASSERT_EQUAL_INT((j * MSG_BUFFER_SIZE) + i, buffer.data[0]);
        }

        TEST_ASSERT_EQUAL_INT(0, message_queue_length(&_msg_queue));
    }
}

static void test_partial_consumption(void)
{
    Message *editable;
    Message buffer;
    uint32_t i;

    /* Add 5 messages */
    for (i = 0; i < 5; i++) {
        editable = message_queue_get_editable(&_msg_queue);
        editable->len = 1;
        editable->data[0] = i;
        message_queue_commit(&_msg_queue);
    }

    TEST_ASSERT_EQUAL_INT(5, message_queue_length(&_msg_queue));

    /* Consume 2 messages */
    message_queue_pop(&_msg_queue, &buffer);
    message_queue_pop(&_msg_queue, &buffer);

    TEST_ASSERT_EQUAL_INT(3, message_queue_length(&_msg_queue));

    /* Add 2 more messages */
    for (i = 0; i < 2; i++) {
        editable = message_queue_get_editable(&_msg_queue);
        editable->len = 1;
        editable->data[0] = 100 + i;
        message_queue_commit(&_msg_queue);
    }

    TEST_ASSERT_EQUAL_INT(5, message_queue_length(&_msg_queue));
}

static void test_max_message_length(void)
{
    Message *editable;
    Message buffer;
    uint32_t i;

    /* Fill message with maximum length */
    editable = message_queue_get_editable(&_msg_queue);
    editable->len = MAX_MSG_LEN;
    for (i = 0; i < MAX_MSG_LEN; i++) {
        editable->data[i] = i % 256;
    }
    message_queue_commit(&_msg_queue);

    /* Pop and verify */
    TEST_ASSERT_EQUAL_INT(1, message_queue_pop(&_msg_queue, &buffer));
    TEST_ASSERT_EQUAL_INT(MAX_MSG_LEN, buffer.len);
    for (i = 0; i < MAX_MSG_LEN; i++) {
        TEST_ASSERT_EQUAL_INT(i % 256, buffer.data[i]);
    }
}

static void test_editable_overwrite(void)
{
    Message *editable;
    Message buffer;

    /* Get and commit first message */
    editable = message_queue_get_editable(&_msg_queue);
    editable->len = 1;
    editable->data[0] = 42;
    message_queue_commit(&_msg_queue);

    /* Get editable again and modify (should point to next slot) */
    editable = message_queue_get_editable(&_msg_queue);
    editable->len = 1;
    editable->data[0] = 99;
    message_queue_commit(&_msg_queue);

    /* Verify both messages */
    TEST_ASSERT_EQUAL_INT(1, message_queue_pop(&_msg_queue, &buffer));
    TEST_ASSERT_EQUAL_INT(42, buffer.data[0]);

    TEST_ASSERT_EQUAL_INT(1, message_queue_pop(&_msg_queue, &buffer));
    TEST_ASSERT_EQUAL_INT(99, buffer.data[0]);
}

static Test *tests_message_buffer_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_init_empty),
        new_TestFixture(test_pop_empty),
        new_TestFixture(test_single_message),
        new_TestFixture(test_multiple_messages),
        new_TestFixture(test_max_capacity),
        new_TestFixture(test_varying_lengths),
        new_TestFixture(test_wrap_around),
        new_TestFixture(test_partial_consumption),
        new_TestFixture(test_max_message_length),
        new_TestFixture(test_editable_overwrite),
    };

    EMB_UNIT_TESTCALLER(message_buffer_tests, NULL, tear_down, fixtures);

    return (Test *)&message_buffer_tests;
}

void tests_message_buffer(void)
{
    TESTS_RUN(tests_message_buffer_tests());
}

/** @} */
