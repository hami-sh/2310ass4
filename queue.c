#include "queue.h"
#include <stdlib.h>

// Starting length of the queue.
const int QUEUE_CAPACITY = 10;

struct Queue new_queue(void) {
    struct Queue output;

    output.data = malloc(sizeof(void*) * QUEUE_CAPACITY);
    output.readEnd = -1; // queue is empty
    output.writeEnd = 0; // put first piece of data at the start of the queue

    return output;
}

void destroy_queue(struct Queue* queue, void (*clean)(void*)) {
    void* data;
    while (read_queue(queue, &data)) {
        clean(data);
    }

    free(queue->data);
}

bool write_queue(struct Queue* queue, void* data) {
    if (queue->writeEnd == queue->readEnd) {
        // queue is full
        return false;
    }

    queue->data[queue->writeEnd] = data;

    // if queue was empty, signal that the queue is no longer empty
    if (queue->readEnd == -1) {
        queue->readEnd = queue->writeEnd;
    }

    queue->writeEnd = (queue->writeEnd + 1) % QUEUE_CAPACITY;

    return true;
}

bool read_queue(struct Queue* queue, void** output) {
    if (queue->readEnd == -1) {
        // queue is empty
        return false;
    }

    *output = queue->data[queue->readEnd];

    queue->readEnd = (queue->readEnd + 1) % QUEUE_CAPACITY;

    if (queue->readEnd == queue->writeEnd) {
        queue->readEnd = -1;
    }

    return true;
}
