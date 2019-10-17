#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "2310depot.h"
#include "comms.h"
#include "channel.h"
#include "queue.h"

#define LINESIZE 500
#define BOLDGREEN "\033[1m\033[32m"
#define RESET "\033[0m"

/**
 * Function to output an error message and return status.
 * @param s status to return with.
 * @return error status.
 */
Status show_message(Status s) {
    const char *messages[] = {"",
            "Usage: 2310depot name {goods qty}\n", 
            "Invalid name(s)\n",
            "Invalid quantity\n"};
    fputs(messages[s], stderr);
    return s;
}

//todo remove perror!

/**
 * Function to check integer for parsing arguments
 * @param string - string to check for integer
 * @return 0 - string was an integer
 *         -1 - string was not an integer / less than 0
 */
int check_int(char *string) {
    if (strlen(string) == 0) {
        return -1;
    }
    // loop over characters of the string
    for (int i = 0; i < strlen(string); i++) {
        // check integer
        if (string[i] == '.' || string[i] == '-') {
            return -1;
        }
        // check that dimensions supplied are digits.
        if (!isdigit(string[i])) {
            return -1;
        }
    }

    // check greater than 0 (for parsing quantity value)
    int numb = atoi(string);
    if (numb < 0) {
        return -1;
    }

    return OK;
}

/**
 * Function to handle the parsing of command line arguments
 * @param argc - number of arguments supplied
 * @param argv - arguments supplied at command line.
 * @param info - depot struct to hold info
 * @return 0 - parsed successfully
 *         1 - Empty name or name contains banned characters
 *         2 - Quantity parameter is < 0 or is not a number
 */
int parse(int argc, char **argv, Depot *info) {
    // check not empty name
    if (strlen(argv[1]) == 0) {
        return show_message(NAMEERR);
    }

    // check if there are illegal chars
    for (int i = 0; i < strlen(argv[1]); i++) {
        if ((argv[1][i] == ' ') || (argv[1][i] == '\n') || (argv[1][i] == '\r')
                || (argv[1][i] == ':')) {
            return show_message(NAMEERR);
        }
    }
    info->name = argv[1];

    /* parse items */
    // malloc based on number of items from the commandline
    info->items = malloc(sizeof(Item) * ((argc - 2) / 2));
    int pos = 0;
    // loop over items (skipping program name and depot name)
    for (int i = 2; i < argc; i++) {
        if (i % 2 == 0) {
            // parse item name (every second argv) & check illegal characters
            Item item;
            for (int j = 0; j < strlen(argv[i]); j++) {
                if ((argv[i][j] == ' ') || (argv[i][j] == '\n')
                        || (argv[i][j] == '\r') || (argv[i][j] == ':')) {
                    return show_message(NAMEERR);
                }
            }
            // store item
            item.name = argv[i];
            info->items[pos] = item;
        } else {
            // parse item quantity.
            int countStatus = check_int(argv[i]);
            if (countStatus != 0) {
                return show_message(QUANERR);
            }
            // store item quantity
            info->items[pos].count = atoi(argv[i]);
            pos++;
        }
    }
    info->totalItems = pos;

    return OK;
}

/**
 * Function to print goods and neighbours lexicographically
 * @param items - array of items to print
 * @param itemSize - size of the item array
 * @param connections - array of connections to the depot
 * @param connectionSize - size of the connection array.
 */
void lexicographic_print(Item *items, int itemSize, Connection *connections,
        int connectionSize) {
    /* print items */
    Item storage;
    for (int i = 0; i < itemSize; ++i) {
        for (int j = i + 1; j < itemSize; ++j) {
            if (strcmp(items[i].name, items[j].name) > 0) {
                // swap if not lexicographically ordered
                storage = items[i];
                items[i] = items[j];
                items[j] = storage;
            }
        }
    }
    // print the items after sorting
    for (int i = 0; i < itemSize; ++i) {
        if (items[i].count != 0) {
            printf("%s %d\n", items[i].name, items[i].count);
            fflush(stdout);
        }
    }

    /* print neighbours */
    printf("Neighbours:\n");
    fflush(stdout);
    Connection temp;
    for (int i = 0; i < connectionSize; ++i) {
        for (int j = i + 1; j < connectionSize; ++j) {
            if (strcmp(connections[i].name, connections[j].name) > 0) {
                // swap if not lexicographically ordered
                temp = connections[i];
                connections[i] = connections[j];
                connections[j] = temp;
            }
        }
    }
    // print neighbours after sorting
    for (int i = 0; i < connectionSize; ++i) {
        if (connections[i].neighbourStatus == 1) {
            printf("%s\n", connections[i].name);
            fflush(stdout);
        }
    }
}

/**
 * Function to handle printing of goods and neighbours when receiving SIGHUP
 * @param data - struct representing depot data.
 */
void sighup_print(Depot *data) {
    printf("Goods:\n");
    fflush(stdout);
    // lock and unlock via mutex
    pthread_mutex_lock(&data->dataLock);
    lexicographic_print(data->items, data->totalItems, data->neighbours,
            data->neighbourCount);
    pthread_mutex_unlock(&data->dataLock);

}

/**
 * Function for thread to read messages from channel and act upon them
 * @param data - void pointer (parsed to ThreadData struct)
 * @return void pointer
 */
void *thread_worker(void *data) {
    // parse ThreadData struct from void*
    ThreadData *thread = (ThreadData *) data;
    while (1) {
        // wait for message
        sem_wait(thread->signal);
        Message *message;
        // read message from the channel
        pthread_mutex_lock(&thread->channelLock);
        read_channel(thread->channel, (void **) &message);
        pthread_mutex_unlock(&thread->channelLock);

//        printf("worker read %s\n", message->input);
//        fflush(stdout);

        // perform function of the message
        if (message->sighup == 1) {
            sighup_print(thread->depot);
        } else {
            process_input(thread->depot, message->input, message->streamTo,
                    message->streamFrom, message->socket);
        }
    }
}

/**
 * Function for thread to listen to connected file streams
 * @param data - void pointer (parsed to ThreadData struct)
 * @return void pointer
 */
void *thread_listen(void *data) {
    // parse ThreadData from void pointer
    ThreadData *depotThread = (ThreadData *) data;
    // send IM message to connected depot
    fprintf(depotThread->streamTo, "IM:%u:%s\n",
            depotThread->depot->listeningPort, depotThread->depot->name);
    fflush(depotThread->streamTo);

    /* read messages from the file stream */
    char input[LINESIZE];
    fgets(input, BUFSIZ, depotThread->streamFrom);
    // continue until EOF from depot (disconnects)
    while (!feof(depotThread->streamFrom)) {
        char *dest = malloc(sizeof(char) * (strlen(input)));
        dest = strncpy(dest, input, strlen(input));

        // create message to send down channel to worker thread
        Message *message = malloc(sizeof(Message));
        message->input = dest;
        message->streamTo = depotThread->streamTo;
        message->streamFrom = depotThread->streamFrom;
        message->socket = depotThread->socket;

        bool output = false;
        while (!output) { // stop once message successfully written
            // properly lock & unlock mutex.
            pthread_mutex_lock(&depotThread->channelLock);
            output = write_channel(depotThread->channel, message);
            pthread_mutex_unlock(&depotThread->channelLock);
        }
        // signal that a message is ready
        sem_post(depotThread->signal);
        fgets(input, BUFSIZ, depotThread->streamFrom);
    }
    return NULL;
}

/**
 * Function to handle incoming connections on the listening port
 * @param info - Depot struct holding related data.
 * @return 0 once
 */
int listening(Depot *info) {
    // place connections in queue.
    if (listen(info->server, SOMAXCONN)) {
        return 0;
    }

    // accept connection
    int connectionFd;
    struct sockaddr_in peerAddr;
    socklen_t addrSize = sizeof(peerAddr);
    while (connectionFd = accept(info->server, (struct sockaddr *) &peerAddr,
            &addrSize), connectionFd >= 0) {
        // get streams
        int dupedFd = dup(connectionFd);
        FILE *streamTo = fdopen(connectionFd, "w");
        FILE *streamFrom = fdopen(dupedFd, "r");

        // prevent same port connection (mutex as sharing memory with worker)
        pthread_mutex_lock(&info->dataLock);
        for (int i = 0; i < info->neighbourCount; i++) {
            if (info->neighbours[i].addr == ntohs(peerAddr.sin_port)) {
                continue;
            }
        }

        // spin up listening thread for the connection
        ThreadData *val = malloc(sizeof(ThreadData));
        val->depot = info;
        val->streamTo = streamTo;
        val->streamFrom = streamFrom;
        val->channel = info->channel;
        val->signal = info->signal;
        val->channelLock = info->channelLock;
        val->socket = connectionFd;
        pthread_mutex_unlock(&info->dataLock);

        pthread_t tid;
        pthread_create(&tid, 0, thread_listen, (void *) val);
        //pthread_join(tid, NULL);
    }
    return 0;
}

/**
 * Function to setup port to listen on.
 * @param info - Depot struct holding related data.
 */
void setup_listen(Depot *info) {
    // create structs for network connection
    struct addrinfo *addrInfo = 0;
    struct addrinfo settings;
    memset(&settings, 0, sizeof(struct addrinfo));
    settings.ai_family = AF_INET;   // IPv4 connection
    settings.ai_socktype = SOCK_STREAM;
    settings.ai_flags = AI_PASSIVE; // bind to the port

    // attempt to get info about the address
    if (getaddrinfo("localhost", 0, &settings, &addrInfo)) {
        freeaddrinfo(addrInfo);
    }

    // create a socket and bind it to a port
    int serv = socket(AF_INET, SOCK_STREAM, 0); // 0 == use default protocol
    bind(serv, (struct sockaddr *) addrInfo->ai_addr, sizeof(struct sockaddr));

    // parse the port from related structs
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    getsockname(serv, (struct sockaddr *) &addr, &len);

    // print the port to stdout & save the data
    printf("%u\n", ntohs(addr.sin_port));
    fflush(stdout);
    info->listeningPort = ntohs(addr.sin_port);
    info->server = serv;

    /* block SIGPIPE */
    signal(SIGPIPE, SIG_IGN);
}

/**
 * Function to allocated required memory
 * @param info - Depot struct holding related data.
 */
void allocate_memory(Depot *info) {
    // initialise deferred command array
    info->deferred = (Deferred *) malloc(1 * sizeof(Deferred *));
    info->defLength = 1;
    info->defCount = 0;

    // initialise neighbour array
    info->neighbours = malloc(500 * sizeof(Connection));
    info->neighbourCount = 0;
    info->neighbourLength = 500;
}

/**
 * Function to add a deferred command
 * @param arr - array of deferred commands
 * @param numElements - number of elements in the deferred command array
 * @param pos - int position to add to
 * @param cmd - Deferred struct containing command to defer.
 */
void add_deferred(Deferred **arr, int *numElements, int *pos, Deferred *cmd) {
    Deferred *temp;
    int tempLength = *numElements + 1;

    /* increment size of list and store element */
    temp = realloc(*arr, tempLength * sizeof(Deferred));
    temp[*pos] = *cmd;
    *pos += 1;
    *arr = temp;
    *numElements = tempLength;
}

/**
 * Function for thread to wait for SIGHUP signal
 * @param info - Depot struct holding related data.
 * @return void pointer
 */
void *sigmund(void *info) {
    // parse Depot struct from void pointer
    Depot *data = (Depot *) info;

    // create message to send down channel for SIGHUP
    Message *message = malloc(sizeof(Message));
    message->sighup = 1;

    // set signal to listen for - SIGHUP
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    int num;
    while (!sigwait(&set, &num)) {  // block here until a signal arrives
        bool output = false;
        // send output down channel (lock appropriately)
        while (!output) {
            pthread_mutex_lock(&data->channelLock);
            output = write_channel(data->channel, message);
            pthread_mutex_unlock(&data->channelLock);
            sem_post(data->signal);
        }
    }
    return 0;
}

/**
 * Function to handle start-up of the depot
 * @param argc - number of arguments supplied
 * @param argv - arguments supplied at command line.
 * @return 0 - normal exit
 *         2 - Empty name or name contains banned characters
 *         3 - Quantity parameter is <0 or not a number
 */
int start_up(int argc, char **argv) {
    Depot info;

    // allocate space for deferred & neighbour lists
    allocate_memory(&info);

    // parse args from commandline
    int parseStatus = parse(argc, argv, &info);
    if (parseStatus != 0) {
        return parseStatus;
    }

    // create mutex for data and semaphore
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    info.dataLock = mutex;
    sem_t signal;
    sem_init(&signal, 0, 0);
    info.signal = &signal;

    // create channel & mutex for channel
    struct Channel *channel = new_channel();
    info.channel = channel;
    pthread_mutex_t channelLock;
    pthread_mutex_init(&channelLock, NULL);
    info.channelLock = channelLock;

    // create thread to listen for SIGHUP signal
    pthread_t tid;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, 0);
    pthread_create(&tid, 0, sigmund, (void *) &info);

    // create worker thread for processing messages
    pthread_t tidWorker;
    ThreadData *worker = malloc(sizeof(ThreadData));
    worker->depot = &info;
    worker->channel = info.channel;
    worker->signal = info.signal;
    worker->channelLock = info.channelLock;
    pthread_create(&tidWorker, 0, thread_worker, (void *) worker);

    // setup listening port
    setup_listen(&info);
    // listen on the port for connections
    listening(&info);

    return OK;
}

/**
 * Function acting as entry point for the program.
 * @param argc - number of arguments received at command line
 * @param argv - array of strings representing arguments received.
 * @return 0 - normal exit
 *         1 - Incorrect number of arguments
 *         2 - Empty name or name contains banned characters
 *         3 - Quantity parameter is < 0 or is not a number
 */
int main(int argc, char **argv) {
    if ((argc % 2) != 0 || argc < 2) { // check correct number of args
        return show_message(INCORRARGS);
    } else {
        // begin the program
        return start_up(argc, argv);
    }
}
