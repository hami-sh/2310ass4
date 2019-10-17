#ifndef COMMS_H
#define COMMS_H
#include "2310depot.h"


void defer_deliver(Depot *info, char *input, char *inputOriginal, int key);

void defer_withdraw(Depot *info, char *input, char *orig, int key);

void defer_transfer(Depot *info, char *input, char *orig, int key);

void add_connection(Connection **list, Connection *connection, int *pos,
        int *numElements);

void process_input(Depot *info, char *input, FILE *in, FILE *out, int socket);

void record_attempt(Depot *info, int socket);

#endif