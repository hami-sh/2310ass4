#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdbool.h>

/*
 * A first in, first out (FIFO) queue. This data structure (by itself) is not
 * threadsafe.
 */
struct Queue {
    // The data contained in the queue - as an array.
    void **data;
    // An offset within that array that points to the beginning of the queue
    // (where new data should be written).
    int writeEnd;
    // An offset within that array that points to the end of the queue (where
    // old data should be read).
    int readEnd;
};

/*
 * Creates (and returns) a new, empty queue, with no data in it.
 */
struct Queue new_queue(void);

/*
 * Destroys an old queue. Takes as arguments a pointer to the queue, as well as
 * a function to use to clean up any elements left in the queue (for example
 * free). If the function provided is NULL, no clean up will be performed on
 * these elements.
 */
void destroy_queue(struct Queue *queue, void (*clean)(void *));

/*
 * Attempts to write a piece of data to the queue. Takes as arguments a pointer
 * to the queue, and the data being written. Returns true if the attempt was
 * successful, and false if the queue was full and unable to be written to.
 */
bool write_queue(struct Queue *queue, void *data);

/*
 * Attempts to read a piece of data from the queue. Takes as arguments a
 * pointer to the queue, and a pointer to where the data should be stored on a
 * successful queue. On success, returns true and sets *output to the read
 * data. On failure (due to empty queue), returns false and does not touch
 * *output.
 */
bool read_queue(struct Queue *queue, void **output);

#endif // _QUEUE_H_
