#ifndef COMMS_H
#define COMMS_H

int defer_deliver(Depot *info, char *input, char *inputOriginal, int key);

int defer_withdraw(Depot *info, char *input, char *orig, int key);

int defer_transfer(Depot *info, char *input, char *orig, int key);

void add_attempt(Connection **list, Connection *item, int *pos, int *numElements);

void process_input(Depot *info, char* input);

#endif