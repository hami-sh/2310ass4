#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>

typedef enum {
    OK = 0,
    INCORRARGS = 1,
    NAMEERR = 2,
    QUANERR = 3
} Status;

// struct for items
typedef struct {
    char* name;
    int count;
} Item;

// struct for connection
typedef struct {
    char* name;
    char* port;
    uint addr;
    FILE* streamTo;
    FILE* streamFrom;
    int neighbourStatus; // 0 for attempted, 1 for confirmed via IM
} Connection;


// struct for the depot
typedef struct {
    char* name;
    Item* items;
    int totalItems;
    int server;
    uint listeningPort;

    Connection* neighbours;
    int neighbourLength;
    int neighbourCount;
} Depot;

// struct for listening thread
typedef struct {
    Depot *depot;
    FILE *streamTo;
    FILE *streamFrom;
} ThreadData;

Status show_message(Status s);

void *thread_listen(void *data);

void record_attempt(Depot *info, int port, FILE *in, FILE *out);

int check_int(char* string);