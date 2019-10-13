#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>

// enum for exit status
typedef enum {
    OK = 0,
    INCORRARGS = 1,
    NAMEERR = 2,
    QUANERR = 3
} Status;

// enum for msgs
typedef enum {
    CONNECT = 0,
    IM = 1,
    DELIVER = 2,
    WITHDRAW = 3,
    TRANSFER = 4,
    DEFD = 5,
    DEFW = 6,
    DEFT = 7,
    EXE = 8
} Msg;

// struct for items
typedef struct {
    char* name;
    int count;
} Item;

// struct for deferred
typedef struct {
    int key;
    Item *item;
    char *location;
    Msg command;
} Deferred;

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

    pthread_mutex_t mutex;

    Deferred *deferred; // int will point to list of def for that key
    int defLength;
    int defCount;
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

void sighup_print(Depot *data);

void add_deferred(Deferred **arr, int *numElements, int *pos, Deferred *cmd);