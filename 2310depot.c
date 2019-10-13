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

#define BOLDGREEN   "\033[1m\033[32m"
#define RESET   "\033[0m"



Status show_message(Status s) {
    const char *messages[] = {"",
            "Usage: 2310depot name {goods qty}\n",
            "Invalid name(s)\n",
            "Invalid quantity\n"};
    fputs(messages[s], stderr);
    return s;
}

//todo remove perror!

int check_int(char* string) {
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
    
    // check greater than 0
    int numb = atoi(string);
    if (numb < 0) {
        return -1;
    }

    return OK;
}

int parse(int argc, char** argv, Depot *info) {
    /* 2310depot garden sand 10 seeds 2 */
    // empty name
    if (strlen(argv[1]) == 0) {
        return show_message(NAMEERR);
    }

    // illegal chars
    for (int i = 0; i < strlen(argv[1]); i++) {
        if ((argv[1][i] == ' ') || (argv[1][i] == '\n') || (argv[1][i] == '\r') 
            || (argv[1][i] == ':')) {
            return show_message(NAMEERR);
        }
    }
    
    // set name
    info->name = argv[1];

    // parse items
    info->items = malloc(sizeof(Item) * ((argc - 2) / 2));
    int pos = 0;
    for (int i = 2; i < argc; i++) {
        if (i % 2 == 0) {
            // parse item name
            Item item;
            for (int j = 0; j < strlen(argv[i]); j++) {
                if ((argv[i][j] == ' ') || (argv[i][j] == '\n')
                    || (argv[i][j] == '\r') || (argv[i][j] == ':')) {
                    return show_message(NAMEERR);
                }
            }
            item.name = argv[i];
            info->items[pos] = item;
        } else {
            // parse item quantity.
            int countStatus = check_int(argv[i]);
            if (countStatus != 0) {
                return show_message(QUANERR);
            }
            info->items[pos].count = atoi(argv[i]);
            pos++;
        }
    }
    info->totalItems = pos;

    return OK;
}

void lexicographic_sort(Item *array, int itemSize, Connection *connections, int connectionSize) {
    Item storage; //todo bruh
    // item print
    for(int i = 0; i < itemSize; ++i) {
        for(int j = i + 1; j < itemSize; ++j) {
            if(strcmp(array[i].name, array[j].name) > 0) {
                storage = array[i];
                array[i] = array[j];
                array[j] = storage;
            }
        }
    }
    for (int i = 0; i < itemSize; ++i) {
        if (array[i].count != 0) {
            printf("%s %d\n", array[i].name, array[i].count);
        }
    }

    // neighbours
    printf("Neighbours:\n");
    Connection temp;
    for(int i = 0; i < connectionSize; ++i) {
        for(int j = i + 1; j < connectionSize; ++j) {
            if(strcmp(connections[i].name, connections[j].name) > 0) {
                temp = connections[i];
                connections[i] = connections[j];
                connections[j] = temp;
            }
        }
    }
    for (int i = 0; i < connectionSize; ++i) {
        if (connections[i].neighbourStatus == 1) {
            printf("%s\n", connections[i].name);
        }
    }
}

void sighup_print(Depot *data) {
    printf("Goods:\n");
    pthread_mutex_lock(&data->mutex);
    lexicographic_sort(data->items, data->totalItems, data->neighbours, data->neighbourCount);
    pthread_mutex_unlock(&data->mutex);
}

void *thread_worker(void *data) {
    ThreadData *thread = (ThreadData*) data;
    while(1) { //todo is this ok?
        sem_wait(thread->signal);
        char *input;
        pthread_mutex_lock(&thread->channelLock);
        read_channel(thread->channel, (void **) &input);
        pthread_mutex_unlock(&thread->channelLock);
        printf("worker read: %s\n", input);
        process_input(thread->depot, input);
    }
}

void *thread_listen(void *data) {
    ThreadData *depotThread = (ThreadData*) data;
    fprintf(depotThread->streamTo, "IM:%u:%s\n", depotThread->depot->listeningPort, depotThread->depot->name);
    fflush(depotThread->streamTo);

    char input[LINESIZE];
    fgets(input, BUFSIZ, depotThread->streamFrom);
    // keep reading until gameover or EOF from hub
    while (!feof(depotThread->streamFrom)) {
        // decide what to do on message
        char* dest = malloc(sizeof(char) * (strlen(input) - 1));
        dest = strncpy(dest, input, strlen(input) - 1);
        printf(BOLDGREEN "---<%s>---\n" RESET, dest);

        bool output = false;
        while(!output) { // stop once successfully written
            pthread_mutex_lock(&depotThread->channelLock);
            output = write_channel(depotThread->channel, input);
            pthread_mutex_unlock(&depotThread->channelLock);
        }
        sem_post(depotThread->signal);

        // get next message
        fgets(input, BUFSIZ, depotThread->streamFrom);
    }

    return NULL;
}

int listening(Depot *info) {
    // place connections in queue.
    if (listen(info->server, SOMAXCONN)) {
        return 0;
    }

    // create channel & mutex for channel
    struct Channel channel = new_channel();
    pthread_mutex_t channelLock;
    pthread_mutex_init(&channelLock, NULL);

    // create worker thread
    pthread_t tid_worker;
    ThreadData *worker = malloc(sizeof(ThreadData));
    worker->depot = info;
    worker->channel = &channel;
    worker->signal = info->signal;
    worker->channelLock = channelLock;
    pthread_create(&tid_worker, 0, thread_worker, (void *)worker);


    int conn_fd;
    struct sockaddr_in peer_addr;
    socklen_t addr_size = sizeof(peer_addr);
    while (conn_fd = accept(info->server, (struct sockaddr *)&peer_addr, &addr_size), conn_fd >= 0) {

        // get streams
        int fd2 = dup(conn_fd);
        FILE* streamTo = fdopen(conn_fd, "w");
        FILE* streamFrom = fdopen(fd2, "r");

        // prevent same port connection todo mutex?
        for (int i = 0; i < info->neighbourCount; i++ ) {
            if (info->neighbours[i].addr == ntohs(peer_addr.sin_port)) {
                continue;
            }
        }

        record_attempt(info, ntohs(peer_addr.sin_port), streamTo, streamFrom);
        // spin up listening thread
        ThreadData *val = malloc(sizeof(ThreadData));
        val->depot = info;
        val->streamTo = streamTo;
        val->streamFrom = streamFrom;
        val->channel = &channel;
        val->signal = info->signal;
        val->channelLock = channelLock;
        pthread_t tid;
        pthread_create(&tid, 0, thread_listen, (void *)val);
        //pthread_join(tid, NULL);
    }
    return 0;

}

void setup_listen(Depot *info) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(& hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;        // IPv6  for generic could use AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;  // Because we want to bind with it
    int err;
    if ((err = getaddrinfo("localhost", 0, &hints, &ai))) { // no particular port
        freeaddrinfo(ai);
        fprintf(stderr, "%s\n", gai_strerror(err));
        //return 1;   // could not work out the address
    }

    // create a socket and bind it to a port
    int serv = socket(AF_INET, SOCK_STREAM, 0); // 0 == use default protocol
    if (bind(serv, (struct sockaddr*)ai->ai_addr, sizeof(struct sockaddr))) {
        perror("Binding");
        //return 3;
    }

    // Which port did we get?
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    socklen_t len=sizeof(struct sockaddr_in);
    if (getsockname(serv, (struct sockaddr*)&addr, &len)) {
        perror("sockname");
        //return 4;
    }
    printf("%u\n", ntohs(addr.sin_port));
    info->listeningPort = ntohs(addr.sin_port);
    info->server = serv;
}

void allocate_memory(Depot *info) {
    // initialise with 1 space
    info->deferred = (Deferred *) malloc(1 * sizeof(Deferred *));
    info->defLength = 1;
    info->defCount = 0;

    info->neighbours = malloc(1 * sizeof(Connection));
    info->neighbourCount = 0;
    info->neighbourLength = 1;
}


void add_deferred(Deferred **arr, int *numElements, int *pos, Deferred *cmd) {
    //todo mutex here
    Deferred *temp;
    int tempLength = *numElements + 1;

    /* increment size of list and store element */
    temp = realloc(*arr, tempLength * sizeof(Deferred));
    temp[*pos] = *cmd;
    *pos += 1;
    *arr = temp;
    *numElements = tempLength;
}

void* sigmund(void* info) {
    Depot *data = (Depot*) info;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    int num;
    while (!sigwait(&set, &num)) {  // block here until a signal arrives
        sighup_print(data);
    }
    return 0;
}


int start_up(int argc, char** argv) {
    Depot info;

    // allocate space for deferred & neighbour lists
    allocate_memory(&info);

    // parse args
    int parseStatus = parse(argc, argv, &info);
    if (parseStatus != 0) {
        return parseStatus;
    }

    pthread_mutex_init(&info.mutex, NULL);
    sem_t signal;
    sem_init(&signal, 0, 0);
    info.signal = &signal;

    // create thread to listen for SIGHUP signal
    pthread_t tid;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, 0);
    pthread_create(&tid, 0, sigmund, (void *)&info);

    // listen
    setup_listen(&info);
    listening(&info);

    return OK;
}

int main(int argc, char** argv) {
    if ((argc % 2) != 0 || argc < 2) { // check correct number of args
        show_message(INCORRARGS);
    } else {
        return start_up(argc, argv);
    }
}
