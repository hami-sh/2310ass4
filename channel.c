#include "channel.h"
#include "queue.h"
#include <stdlib.h>

/**
 * Function to create a channel for thread safe communication
 * @return struct Channel - the new channel
 */
struct Channel *new_channel(void) {
    // malloc space
    struct Channel *output = malloc(sizeof(struct Channel));

    // create a new queue
    output->inner = new_queue();

    return output;
}

/**
 * Function to destroy a channel
 * @param channel - struct Channel to destroy
 * @param clean - function pointer to clean up elements within the queue
 */
void destroy_channel(struct Channel *channel, void (*clean)(void *)) {
    destroy_queue(&channel->inner, clean);
}

/**
 * Function to write to the channel
 * @param channel - struct Channel to write to
 * @param data - void * data to write into the channel
 * @return false if unsuccessful
 *         true if successful
 */
bool write_channel(struct Channel *channel, void *data) {
    // attempt to write to the queue
    bool output = write_queue(&channel->inner, data);

    return output;
}

/**
 * Function to read from the channel
 * @param channel - struct Channel to read from
 * @param out - void ** to write to
 * @return false if read unsuccessful
 *         true if read successful
 */
bool read_channel(struct Channel *channel, void **out) {
    // attempt to write
    bool output = read_queue(&channel->inner, out);

    return output;
}
