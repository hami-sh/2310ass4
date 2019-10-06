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

int check_int(char* string) {
    for (int i = 0; i < strlen(string); i++) {
        // check integer
        if (string[i] == '.' || string[i] == '-') {
            return show_message(QUANERR);
        } 
        // check that dimensions supplied are digits.
        if (!isdigit(string[i])) {
            return show_message(QUANERR);
        }
    }
    
    // check greater than 0
    int numb = atoi(string);
    if (numb < 0) {
        return show_message(QUANERR);
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
                return countStatus;
            }
            info->items[pos].count = atoi(argv[i]);
            pos++;
        }
    }
    info->totalItems = pos;

    return OK;
}

void growArray(Depot *info, Item **items, int currentSize) {
    printf("--grow\n");
    int totalSize = currentSize + 1;
    Item *temp = (Item*)realloc(*items, (totalSize * sizeof(Item)));

    if (temp == NULL) {
        //todo remove
        printf("realloc error\n");
        return;
    } else {
        *items = temp;
    }

    info->totalItems = totalSize;
}

void item_add(Depot *info, Item *new) {
    printf("--add attempt\n");
    int found = 0;

    for (int i = 0; i < info->totalItems; i++) {
        if (strcmp(info->items[i].name, new->name) == 0) {
            found = 1;
            info->items[i].count += new->count;
        }
    }

    if (found == 0) {
        growArray(info, &info->items, info->totalItems);
        printf("(new size %d)\n", info->totalItems);
        info->items[info->totalItems - 1] = *new;
    }

}

void depot_connect(Depot *info, char* input) {
    input += 7;
    if (input[0] != ':') {
        return;
    }
    input++;

    char* port = malloc(sizeof(char) * strlen(input) - 1);
    strncpy(port, input, strlen(input) - 1);

    // connect to port.
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(& hints, 0, sizeof(struct addrinfo));
    hints.ai_family=AF_INET;        // IPv6  for generic could use AF_UNSPEC
    hints.ai_socktype=SOCK_STREAM;
    int err;
    if ((err=getaddrinfo("localhost", port, &hints, &ai))) {
        freeaddrinfo(ai);
        return;   // could not work out the address
    }
    int fd=socket(AF_INET, SOCK_STREAM, 0); // 0 == use default protocol
    if (connect(fd, (struct sockaddr*)ai->ai_addr, sizeof(struct sockaddr))) {
        return;
    }
    // fd is now connected
    // we want separate streams (which we can close independently)
    
    int fd2=dup(fd);
    FILE* to=fdopen(fd, "w");
    FILE* from=fdopen(fd2, "r");
    Connection *connection = malloc(sizeof(Connection));
    connection->port = port;
    connection->streamFrom = from;
    connection->streamTo = to;

    info->connections[info->currentConnected] = *connection;
    fprintf(to, "IM:%u:%s\n", info->listeningPort, info->name);
    fflush(to);
}

void depot_im(Depot *info, char* input) {
    // send IM back?

    // store connection info
}

void depot_deliver(Depot *info, char* input) {
    input += 7;
    if (input[0] != ':') {
        return;
    }
    input++;
    int numberDigits = 0;
    int q = 0;
    while (input[q] != ':') {
        numberDigits++;
        q++;
    }
    char* quanOrig = malloc(sizeof(char) * numberDigits);
    strncpy(quanOrig, input, numberDigits);
    for (int i = 0; i < numberDigits; i++) {
        if (!isdigit(quanOrig[i])) {
            return;
        }
    }

    int quantity = atoi(quanOrig);
    input += numberDigits;
    if (input[0] != ':') {
        return;
    }
    input++;

    char* itemName = malloc(sizeof(char) * strlen(input));
    strcpy(itemName, input);
    itemName[strlen(itemName) - 1] = '\0';

    if (quantity <= 0) {
        return;
    }

    printf("in: %s %d\n", itemName, quantity);
    Item *new = malloc(sizeof(Item));
    new->name = itemName;
    new->count = quantity;

    item_add(info, new);

    //todo DEBUG REMOVE
    for (int i = 0; i < info->totalItems; i++) {
        printf("%s:%d\n", info->items[i].name, info->items[i].count);
    }


}

void depot_withdraw(Depot *info, char* input) {

}

void depot_transfer(Depot *info, char* input) {

}

void process_input(Depot *info, char* input) {
    char *dest = malloc(sizeof(char) * strlen(input));

    if (strncmp(input, "Connect", 7) == 0) {
        // strncpy(dest, input, 4);
        // dest[4] = 0; 
        printf("GOT: Connect\n");
        depot_connect(info, input);
    } else if (strncmp(input, "IM", 2) == 0) {
        // strncpy(dest, input, 8);
        // dest[8] = 0;
        printf("GOT: IM\n");
        depot_im(info, input);
    } else if (strncmp(input, "Deliver", 7) == 0) {
        printf("GOT: Deliver \n");
        depot_deliver(info, input);
    } else if (strncmp(input, "Withdraw", 8) == 0) {
        printf("Withdraw\n");
    } else if (strncmp(input, "Transfer", 8) == 0) {
        printf("Transfer\n");
    } else if (strncmp(input, "Defer", 5) == 0) {
        printf("Defer\n");
    } else if (strncmp(input, "Execute", 7) == 0) {
        printf("Execute\n");
    }
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
    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len=sizeof(struct sockaddr_in);
    if (getsockname(serv, (struct sockaddr*)&ad, &len)) {
        perror("sockname");
        //return 4;
    }
    printf("%u\n", ntohs(ad.sin_port));
    info->listeningPort = ntohs(ad.sin_port);
    info->server = serv;
}

void *thread_listen(void *data) {
    DepotThread *depotThread = (DepotThread*) data;

    char input[LINESIZE];
    fgets(input, BUFSIZ, depotThread->streamFrom);
    // keep reading until gameover or EOF from hub
    while (!feof(depotThread->streamFrom)) {
        // decide what to do on message
        // int processed = process_input(input, game);
        // if (processed != 0) {
            // return processed;
        // }
        process_input(depotThread->depot, input);

        // get next message
        fgets(input, BUFSIZ, depotThread->streamFrom);
    }
}

int listening(Depot *info) {
    if (listen(info->server, 10)) {     // allow up to 10 connection requests to queue
        perror("Listen");
        // return 4;
    }
    
    int conn_fd;
    while (conn_fd = accept(info->server, 0, 0), conn_fd >= 0) {    // change 0, 0 to get info about other end
        // FILE* stream = fdopen(conn_fd, "w");
        // fputs(msg, stream);
        // fflush(stream);
        // fclose(stream);

        // do something with the connection
        printf("--CONNECTION--\n");

        // get streams
        int fd2 = dup(conn_fd);
        FILE* streamTo = fdopen(conn_fd, "w");
        FILE* streamFrom = fdopen(fd2, "r");

        // print introduction
        fprintf(streamTo, "IM:%u:%s\n", info->listeningPort, info->name);
        fflush(streamTo);

        // spin up listening thread
        printf("thread open\n");
        pthread_t tid;
        DepotThread *val = malloc(sizeof(DepotThread));
        val->depot = info;
        val->streamTo = streamTo;
        val->streamFrom = streamFrom;
        pthread_create(&tid, 0, thread_listen, (void *)val);
        pthread_join(tid, NULL);
        printf("thread close\n");
    }
    return 0;

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

    info.connections = malloc(3 * sizeof(Connection)); //todo realloc!
    info.currentConnected = 0;
    info.connectionNum = 3;

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
