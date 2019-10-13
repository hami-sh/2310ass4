#include <pthread.h>
#include "2310depot.h"
#include "comms.h"

void growArray(Depot *info, Item **items, int currentSize) {
    int totalSize = currentSize + 1;
    Item *temp = (Item*)realloc(*items, (totalSize * sizeof(Item)));

    if (temp == NULL) {
        return;
    } else {
        *items = temp;
    }

    info->totalItems = totalSize;
}

void item_add(Depot *info, Item *new) {
//    printf("--add attempt\n");
    int found = 0;

    for (int i = 0; i < info->totalItems; i++) {
        if (strcmp(info->items[i].name, new->name) == 0) {
            found = 1;
            info->items[i].count += new->count;
        }
    }

    if (found == 0) {
        growArray(info, &info->items, info->totalItems);
        info->items[info->totalItems - 1] = *new;
    }

}

void item_remove(Depot *info, Item *new) {
//    printf("--withdraw attempt\n");
    int found = 0;

    for (int i = 0; i < info->totalItems; i++) {
        if (strcmp(info->items[i].name, new->name) == 0) {
            found = 1;
            info->items[i].count -= new->count;
        }
    }

    if (found == 0) {
        new->count = new->count * -1;
        item_add(info, new);
    }

}

void add_to_list(Connection* list, Connection item, int pos, int length) {
    //todo realloc & mutex!
    list[pos] = item;
}

void record_attempt(Depot *info, int port, FILE *in, FILE *out) {
    Connection *server = malloc(sizeof(Connection));
    pthread_mutex_lock(&info->mutex);
    server->addr = port;
    server->neighbourStatus = 0;
    server->streamTo = in;
    server->streamFrom = out;
    add_to_list(info->neighbours, *server, info->neighbourCount++, info->neighbourLength);
    pthread_mutex_unlock(&info->mutex);
}

void depot_connect(Depot *info, char* input) {
    input += 7;
    if (input[0] != ':') {
        return;
    }
    input++;

    char* port = malloc(sizeof(char) * strlen(input) - 1);
    strncpy(port, input, strlen(input) - 1);

    if (check_int(port) != 0) {
        return;
    }

    // prevent same port connection todo mutex?
    pthread_mutex_lock(&info->mutex);
    for (int i = 0; i < info->neighbourCount; i++ ) {
        if (info->neighbours[i].addr == atoi(port)) {
            return;
        }
    }
    pthread_mutex_unlock(&info->mutex);

    // connect to port.
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(& hints, 0, sizeof(struct addrinfo));
    hints.ai_family=AF_INET;
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

    // spin up listening thread
//    printf("thread open\n");
    pthread_t tid;
    ThreadData *val = malloc(sizeof(ThreadData));
    pthread_mutex_lock(&info->mutex);
    val->depot = info;
    pthread_mutex_unlock(&info->mutex);
    val->streamTo = to;
    val->streamFrom = from;

    // record attempted connection
    record_attempt(info, atoi(port), to, from);

    printf("success\n");
    pthread_create(&tid, 0, thread_listen, (void *)val);
}

int check_illegal_char(char* input, Msg msg) {
    int counter = 0;
    for (int i = 0; i < strlen(input); i++) {
        if (input[i] == ':') {
            counter++;
        }
        if (input[i] == '\r' || input[i] == ' ') {
            return -1;
        }
        if (input[i] == '\n' && ((i != strlen(input) - 1) || i != strlen(input))) {
            printf("%d %d\n", i, strlen(input));
            return -1;
        }
    }
    if (msg == IM || msg == DELIVER || msg == WITHDRAW) {
        if (counter != 2) {
            return -1;
        }
    }
    if (msg == TRANSFER) {
        if (counter != 3) {
            return -1;
        }
    }
    if (msg == DEFD || msg == DEFW) {
        if (counter != 4) {
            return -1;
        }
    }
    if (msg == DEFT) {
        if (counter != 5) {
            return -1;
        }
    }

    return 0;
}

void depot_im(Depot *info, char* input) {
    input[strlen(input) - 1] = '\0';
    int checked = check_illegal_char(input, IM);
    if (checked != 0) {
        return;
    }
    input += 2;
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
    char* portOrig = malloc(sizeof(char) * numberDigits);
    strncpy(portOrig, input, numberDigits);
    for (int i = 0; i < numberDigits; i++) {
        if (!isdigit(portOrig[i])) {
            return;
        }
    }

    int port = atoi(portOrig);
    input += numberDigits;
    if (input[0] != ':') {
        return;
    }
    input++;

    char* serverName = malloc(sizeof(char) * strlen(input));
    strcpy(serverName, input);
//    serverName[strlen(serverName) - 1] = '\0';

    // store connection as neighbour
    for (int i = 0; i < info->neighbourCount; i++) {
        if (info->neighbours[i].addr == port) {
            info->neighbours[i].neighbourStatus = 1;
            info->neighbours[i].name = serverName;
        }
    }
}

int depot_deliver(Depot *info, char* input, int key) {
    if (key == -1) {
        input[strlen(input) - 1] = '\0';
    }
    int checked = check_illegal_char(input, DELIVER);
    if (checked != 0) {
        return 0;
    }
    input += 7;
    if (input[0] != ':') {
        return 0;
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
            return show_message(QUANERR);
        }
    }

    int quantity = atoi(quanOrig);
    input += numberDigits;
    if (input[0] != ':') {
        return 0;
    }
    input++;

    char* itemName = malloc(sizeof(char) * strlen(input));
    strcpy(itemName, input);

    if (quantity <= 0) {
        return show_message(QUANERR);
    }
    if (strlen(itemName) == 0) {
        return 0;
    }

    Item *new = malloc(sizeof(Item));
    new->name = itemName;
    new->count = quantity;

    if (key == -1) {
        // instant
        printf("in: %s %d\n", itemName, quantity);
        item_add(info, new);
    } else {
        // defer action
        printf("def in: %s %d <%d>\n", itemName, quantity, key);
        Deferred *cmd = malloc(sizeof(Deferred));
        cmd->key = key;
        cmd->command = DELIVER;
        cmd->item = new;
        add_deferred(&info->deferred, &info->defLength, &info->defCount, cmd);

    }

    //todo DEBUG REMOVE
    for (int i = 0; i < info->totalItems; i++) {
        printf("%s:%d\n", info->items[i].name, info->items[i].count);
    }
    return 0;

}

int depot_withdraw(Depot *info, char* input, int key) {
    if (key == -1) {
        input[strlen(input) - 1] = '\0';
    }
    int checked = check_illegal_char(input, WITHDRAW);
    if (checked != 0) {
        return 0;
    }
    input += 8;
    if (input[0] != ':') {
        return 0;
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
            return show_message(QUANERR);
        }
    }

    int quantity = atoi(quanOrig);
    input += numberDigits;
    if (input[0] != ':') {
        return 0;
    }
    input++;

    char* itemName = malloc(sizeof(char) * strlen(input));
    strcpy(itemName, input);

    if (quantity <= 0) {
        return show_message(QUANERR);
    }
    if (strlen(itemName) == 0) {
        return 0;
    }

    printf("out: %s %d\n", itemName, quantity);
    Item *new = malloc(sizeof(Item));
    new->name = itemName;
    new->count = quantity;

    if (key == -1) {
        // instant
        item_remove(info, new);
    } else {
        // defer
        Deferred *cmd = malloc(sizeof(Deferred));
        cmd->key = key;
        cmd->command = WITHDRAW;
        cmd->item = new;
        add_deferred(&info->deferred, &info->defLength, &info->defCount, cmd);
    }

    //todo DEBUG REMOVE
    for (int i = 0; i < info->totalItems; i++) {
        printf("%s:%d\n", info->items[i].name, info->items[i].count);
    }
    return 0;
}

int depot_transfer(Depot *info, char* input, int key) {
    if (key == -1) {
        input[strlen(input) - 1] = '\0';
    }
    int checked = check_illegal_char(input, TRANSFER);
    if (checked != 0) {
        return 0;
    }
    input += 8;
    if (input[0] != ':') {
        return 0;
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
            return show_message(QUANERR);
        }
    }

    int quantity = atoi(quanOrig);
    input += numberDigits;
    if (input[0] != ':') {
        return 0;
    }
    input++;

    int itemLength = 0;
    int j = 0;
    while (input[j] != ':') {
        itemLength++;
        j++;
    }
    char* itemName = malloc(sizeof(char) * itemLength);
    strncpy(itemName, input, itemLength);
    input += itemLength;

    if (input[0] != ':') {
        return 0;
    }
    input++;

    char* serverName = malloc(sizeof(char) * strlen(input));
    strcpy(serverName, input);

    if (quantity <= 0) {
        return show_message(QUANERR);
    } else if (strlen(itemName) == 0 || strlen(serverName) == 0) {
        return 0;
    }

    printf("transfer %d %s to %s\n", quantity, itemName, serverName);

    // check if depot present
    FILE *stream = NULL;
    for (int i = 0; i < info->neighbourCount; i++) {
        if (info->neighbours[i].name != NULL) {
            if (strcmp(info->neighbours[i].name, serverName) == 0) {
                stream = info->neighbours[i].streamTo;
                break;
            }
        }
    }
    if (stream == NULL) {
        return 0;
    }

    // remove from us, add to them
    Item *item = malloc(sizeof(Item));
    item->name = itemName;
    item->count = quantity;
    if (key == -1) {
        // instant
        item_remove(info, item);
        fprintf(stream, "Deliver:%d:%s\n", item->count, item->name);
        fflush(stream);
    } else {
        // defer
        Deferred *cmd = malloc(sizeof(Deferred));
        cmd->key = key;
        cmd->command = TRANSFER;
        cmd->item = item;
        cmd->location = serverName;
        add_deferred(&info->deferred, &info->defLength, &info->defCount, cmd);
    }


    return 0;
}

int defer(Depot *info, char *input) {
    input[strlen(input) - 1] = '\0';
    char *inputOrig = malloc(sizeof(char) * strlen(input));
    strcpy(inputOrig, input);

    input += 5; // remove starting portion of msg
    if (input[0] != ':') {
        return 0; // check formatting
    }
    input++;

    // get key
    int numberDigits = 0;
    while (input[numberDigits] != ':') {
        numberDigits++;
    }
    char* keyOrig = malloc(sizeof(char) * numberDigits);
    strncpy(keyOrig, input, numberDigits);
    for (int i = 0; i < numberDigits; i++) {
        if (!isdigit(keyOrig[i])) {
            return 0;
        }
    }

    int key = atoi(keyOrig);
    input += numberDigits;
    if (input[0] != ':') {
        return 0; // check formatting
    }
    input++;

    // get order
    int numberLetters = 0;
    while (input[numberLetters] != ':') {
        numberLetters++;
    }
    char* order = malloc(sizeof(char) * numberLetters);
    strncpy(order, input, numberLetters);

    // store details at key
    printf("%s <%d>\n", order, key);
    if (strcmp(order, "Deliver") == 0) {
        return defer_deliver(info, input, inputOrig, key);
    } else if (strcmp(order, "Withdraw") == 0) {
        return defer_withdraw(info, input, inputOrig, key);
    } else if (strcmp(order, "Transfer") == 0) {
        return defer_transfer(info, input, inputOrig, key);
    }
    return 0;

}

int defer_deliver(Depot *info, char *input, char *orig, int key) {
    int checked = check_illegal_char(orig, DEFD);
    if (checked != 0) {
        return 0;
    }
    return depot_deliver(info, input, key);
}

int defer_withdraw(Depot *info, char *input, char *orig, int key) {
    int checked = check_illegal_char(orig, DEFW);
    if (checked != 0) {
        return 0;
    }
    return depot_withdraw(info, input, key);
}

int defer_transfer(Depot *info, char *input, char *orig, int key) {
    int checked = check_illegal_char(orig, DEFT);
    if (checked != 0) {
        return 0;
    }
    return depot_transfer(info, input, key);
}

void debug(Depot *info) {
    printf("--------(DEBUG)--------\n");
    printf("%s:%u\n", info->name, info->listeningPort);
    printf("Goods:\n");
    for (int i = 0; i < info->totalItems; i++) {
        printf("%s:%d\n", info->items[i].name, info->items[i].count);
    }
    printf("Neighbours:\n");
    for (int i = 0; i < info->neighbourCount; i++) {
        if (info->neighbours[i].neighbourStatus == 0) {
            printf("%u\n", info->neighbours[i].addr);
        } else {
            printf("%u <%s>\n", info->neighbours[i].addr, info->neighbours[i].name);
        }
    }

    printf("Deferred: ");
    printf("2:deliver, 3:widthdraw, 4:transfer\n");
    for (int i = 0; i < info->defCount; i++) {
        if (info->deferred[i].command == TRANSFER) {
            printf("<%d> %d %s:%d to %s\n", info->deferred[i].key, info->deferred[i].command,
                    info->deferred[i].item->name, info->deferred[i].item->count, info->deferred[i].location);
        } else {
            printf("<%d> %d %s:%d\n", info->deferred[i].key, info->deferred[i].command,
                    info->deferred[i].item->name, info->deferred[i].item->count);
        }
    }
    printf("-----------------------\n");

}

void process_input(Depot *info, char* input) {
    if (strncmp(input, "Connect", 7) == 0) {
        // strncpy(dest, input, 4);
        // dest[4] = 0;
//        printf("GOT: Connect\n");
        depot_connect(info, input);
    } else if (strncmp(input, "IM", 2) == 0) {
        // strncpy(dest, input, 8);
        // dest[8] = 0;
//        printf("GOT: IM\n");
        depot_im(info, input);
    } else if (strncmp(input, "Deliver", 7) == 0) {
//        printf("GOT: Deliver \n");
        depot_deliver(info, input, -1);
    } else if (strncmp(input, "Withdraw", 8) == 0) {
//        printf("Withdraw\n");
        depot_withdraw(info, input, -1);
    } else if (strncmp(input, "Transfer", 8) == 0) {
//        printf("Transfer\n");
        depot_transfer(info, input, -1);
    } else if (strncmp(input, "Defer", 5) == 0) {
//        printf("Defer\n");
        defer(info, input);
    } else if (strncmp(input, "Execute", 7) == 0) {
//        printf("Execute \n");
    } else if (strncmp(input, "debug", 5) == 0) {
        debug(info); //todo REMOVE
    } else if (strncmp(input, "sig", 3) == 0) {
        sighup_print(info);
    }
}
