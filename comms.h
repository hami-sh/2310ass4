// #include "2310depot.h"
// void process_input(Depot *info, char* input);
// void depot_connect(Depot *info, char* input);

int defer_deliver(Depot *info, char *input, char *inputOriginal, int key);

int defer_withdraw(Depot *info, char *input, char *orig, int key);

int defer_transfer(Depot *info, char *input, char *orig, int key);

void add_attempt(Connection **list, Connection *item, int *pos, int *numElements);

void process_input(Depot *info, char* input);