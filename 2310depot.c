#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include "2310depot.h"
#include "comms.h"

#define LINESIZE 500

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

void *thread_listen(void *data) {
    ThreadData *depotThread = (ThreadData*) data;
    fprintf(depotThread->streamTo, "IM:%u:%s\n", depotThread->depot->listeningPort, depotThread->depot->name);
    fflush(depotThread->streamTo);

    char input[LINESIZE];
    fgets(input, BUFSIZ, depotThread->streamFrom);
    // keep reading until gameover or EOF from hub
    while (!feof(depotThread->streamFrom)) {
        // decide what to do on message
        // int processed = process_input(input, game);
        // if (processed != 0) {
            // return processed;
        // }
        char* dest = malloc(sizeof(char) * (strlen(input) - 1));
        dest = strncpy(dest, input, strlen(input) - 1);
        printf("---<%s>---\n", dest);
        process_input(depotThread->depot, input);

        // get next message
        fgets(input, BUFSIZ, depotThread->streamFrom);
    }
}

int listening(Depot *info) {
    // place connections in queue.
    if (listen(info->server, SOMAXCONN)) {
        return 0;
    }

    int conn_fd;
    struct sockaddr_in peer_addr;
    socklen_t addr_size = sizeof(peer_addr);
    while (conn_fd = accept(info->server, (struct sockaddr *)&peer_addr, &addr_size), conn_fd >= 0) {    // change 0, 0 to get info about other end

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
        printf("thread open\n");
        ThreadData *val = malloc(sizeof(ThreadData));
        val->depot = info;
        val->streamTo = streamTo;
        val->streamFrom = streamFrom;
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

int start_up(int argc, char** argv) {
    Depot info;

    // parse args
    int parseStatus = parse(argc, argv, &info);
    if (parseStatus != 0) {
        return parseStatus;
    }

    //todo DEBUG REMOVE
    for (int i = 0; i < info.totalItems; i++) {
        printf("%s:%d\n", info.items[i].name, info.items[i].count);
    }

    info.neighbours = malloc(3 * sizeof(Connection)); //todo realloc!
    info.neighbourCount = 0;
    info.neighbourLength = 3;

    info.neighbours = malloc(3 * sizeof(Connection)); //todo realloc!
    info.neighbourCount = 0;
    info.neighbourLength = 3;

    // listen 
    setup_listen(&info);
    listening(&info);

    
    return OK;

}


int main(int argc, char** argv) {
    if ((argc % 2) != 0 || argc < 2) {
        show_message(INCORRARGS);
    } else {
        return start_up(argc, argv);
    }
}
