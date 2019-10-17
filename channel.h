#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include "queue.h"
#include <stdbool.h>

/*
 * A threadsafe channel. Data can be written to the channel or read from the
 * channel at different times, by different threads - safely.
 */
struct Channel {
    struct Queue inner;
};

/*
 * Creates (and returns) a new, empty channel, with no data in it.
 */
struct Channel *new_channel(void);

/*
 * Destroys an old channel. Takes as arguments a pointer to the channel, as
 * well as a function to use to clean up any elements left in the channel (for
 * example free). If the function provided is NULL, no clean up will be
 * performed on these elements.
 */
void destroy_channel(struct Channel *channel, void (*clean)(void *));

/*
 * Attempts to write a piece of data to the channel. Takes as arguments a
 * pointer to the channel, and the data being written. Returns true if the
 * attempt was successful, and false if the channel was full and unable to be
 * written to.
 */
bool write_channel(struct Channel *channel, void *data);

/*
 * Attempts to read a piece of data from the channel. Takes as arguments a
 * pointer to the channel, and a pointer to where the data should be stored on
 * a successful channel. On success, returns true and sets *output to the read
 * data. On failure (due to empty channel), returns false and does not touch
 * *output.
 */
bool read_channel(struct Channel *channel, void **output);

#endif // _CHANNEL_H_
