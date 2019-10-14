#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include "channel.h"
#include <semaphore.h>

#ifndef DEPOT_H
#define DEPOT_H

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
} Command;

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
    Command command;
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
    sem_t *signal;

    struct Channel *channel;


    Deferred *deferred; // int will point to list of def for that key
    int defLength;
    int defCount;
} Depot;

// struct for listening thread
typedef struct {
    Depot *depot;
    FILE *streamTo;
    FILE *streamFrom;
    struct Channel *channel;
    pthread_mutex_t lock;
    pthread_mutex_t channelLock;
    sem_t *signal;
} ThreadData;

// struct for message down channel
typedef struct {
    char* input;
    FILE* streamTo;
    FILE* streamFrom;
} Message;


Status show_message(Status s);

void *thread_listen(void *data);

int check_int(char* string);

void sighup_print(Depot *data);

void add_deferred(Deferred **arr, int *numElements, int *pos, Deferred *cmd);

#endif
