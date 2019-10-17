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
    char *name;
    int count;
} Item;

// struct for deferred
typedef struct {
    int key;
    Item *item;
    char *location;
    Command command;
    char *input;
} Deferred;

// struct for connection
typedef struct {
    char *name;
    char *port;
    uint addr;
    FILE *streamTo;
    FILE *streamFrom;
    int neighbourStatus; // 0 for attempted, 1 for confirmed via IM
} Connection;


// struct for the depot
typedef struct {
    char *name;
    Item *items;
    int totalItems;
    int server;
    uint listeningPort;

    Connection *attempts;
    int attemptLength;
    int attemptCount;

    Connection *neighbours;
    int neighbourLength;
    int neighbourCount;

    pthread_mutex_t dataLock;
    sem_t *signal;

    struct Channel *channel;
    pthread_mutex_t channelLock;


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
    int socket; // fd for socket
    int ignore; // ignore further messages
    int address; // which address did it arrive from
} ThreadData;

// struct for message down channel
typedef struct {
    char *input;
    FILE *streamTo;
    FILE *streamFrom;
    int socket;
    int sighup; //whether to print sighup
    int address; // address of depot
} Message;


Status show_message(Status s);

void *thread_listen(void *data);

int check_int(char *string);

void sighup_print(Depot *data);

void add_deferred(Deferred **arr, int *numElements, int *pos, Deferred *cmd);

#endif
