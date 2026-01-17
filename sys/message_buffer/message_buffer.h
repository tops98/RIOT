#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifndef MAX_MSG_LEN
    #define MAX_MSG_LEN 100
#endif

#ifndef MSG_BUFFER_SIZE
    #define MSG_BUFFER_SIZE 10
#endif

typedef struct {
    uint8_t data[MAX_MSG_LEN];
    uint32_t len;
} Message;

typedef struct {
    Message msg[MSG_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    bool is_full;
} MessageQueue;

// Function declarations
bool message_queue_pop(MessageQueue *q, Message *buffer);
void message_queue_commit(MessageQueue *q);
Message* message_queue_get_editable(MessageQueue *q);
uint32_t message_queue_length(MessageQueue *q);

#endif // MESSAGE_BUFFER_H
