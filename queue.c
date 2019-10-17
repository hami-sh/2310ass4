#include "queue.h"
#include <stdlib.h>

// Starting length of the queue.
const int queueCapacity = 10;

/**
 * Function to create a new queue
 * @return struct Queue just created
 */
struct Queue new_queue(void) {
    struct Queue output;

    output.data = malloc(sizeof(void *) * queueCapacity);
    output.readEnd = -1; // queue is empty
    output.writeEnd = 0; // put first piece of data at the start of the queue

    return output;
}

/**
 * Function to destroy a queue
 * @param queue - struct Queue to destroy
 * @param clean - function to clean elements remaining in the queue
 */
void destroy_queue(struct Queue *queue, void (*clean)(void *)) {
    void *data;
    // remove all data
    while (read_queue(queue, &data)) {
        clean(data);
    }

    free(queue->data);
}

/**
 * Function to write to the queue
 * @param queue - struct Queue to write to
 * @param data - void * data to write into the queue
 * @return false if queue full
 *         true if queue empty (write successful)
 */
bool write_queue(struct Queue *queue, void *data) {
    if (queue->writeEnd == queue->readEnd) {
        // queue is full
        return false;
    }

    queue->data[queue->writeEnd] = data;

    // if queue was empty, signal that the queue is no longer empty
    if (queue->readEnd == -1) {
        queue->readEnd = queue->writeEnd;
    }

    queue->writeEnd = (queue->writeEnd + 1) % queueCapacity;

    return true;
}

/**
 * Function to read from the queue
 * @param queue - struct Queue to read from
 * @param output - void ** to write to
 * @return false if queue empty (read fail)
 *         true if element present (read success)
 */
bool read_queue(struct Queue *queue, void **output) {
    if (queue->readEnd == -1) {
        // queue is empty
        return false;
    }

    *output = queue->data[queue->readEnd];

    queue->readEnd = (queue->readEnd + 1) % queueCapacity;

    if (queue->readEnd == queue->writeEnd) {
        queue->readEnd = -1;
    }

    return true;
}
