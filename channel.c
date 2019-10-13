#include "channel.h"
#include "queue.h"

struct Channel new_channel(void) {
    struct Channel output;

    output.inner = new_queue();

    return output;
}

void destroy_channel(struct Channel* channel, void (*clean)(void*)) {
    destroy_queue(&channel->inner, clean);
}

bool write_channel(struct Channel* channel, void* data) {

    bool output = write_queue(&channel->inner, data);

    return output;
}

bool read_channel(struct Channel* channel, void** out) {

    bool output = read_queue(&channel->inner, out);

    return output;
}
