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
    FILE* streamTo;
    FILE* streamFrom;
} Connection;


// struct for the depot
typedef struct {
    char* name;
    Item* items;
    int totalItems;
    int server;
    uint listeningPort;

    Connection* connections; 
    int connectionNum;
    int currentConnected;
} Depot;

// struct for listening thread
typedef struct {
    Depot *depot;
    FILE *streamTo;
    FILE *streamFrom;

} DepotThread;

void *thread_listen(void *data);