#include "message_buffer.h"

// WARNING NOT THREAD SAFE!!!

bool message_queue_pop(MessageQueue *q, Message *buffer) {
    // check if queue is empty
    if (!q->is_full && q->head == q->tail) {
        return false;
    }

    buffer->len = q->msg[q->tail].len;
    for (uint32_t i = 0; i < q->msg[q->tail].len; i++) {
        buffer->data[i] = q->msg[q->tail].data[i];
    }

    q->tail = (q->tail + 1) % MSG_BUFFER_SIZE;
    q->is_full = false;
    return true;
}

void message_queue_commit(MessageQueue *q) {
    q->head = (q->head + 1) % MSG_BUFFER_SIZE;
    if(q->head == q->tail) {
        q->is_full = true;
    }
}

Message* message_queue_get_editable(MessageQueue *q) {
    return &q->msg[q->head];
}

uint32_t message_queue_length(MessageQueue *q) {
    if(q->is_full) {
        return MSG_BUFFER_SIZE;
    }
    if (q->head >= q->tail) {
        return q->head - q->tail;
    } else {
        return MSG_BUFFER_SIZE - q->tail + q->head;
    }
}
