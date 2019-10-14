#include <pthread.h>
#include "2310depot.h"
#include "comms.h"
#include "channel.h"

/**
 * increase the size of the item array
 * @param info - Depot struct holding related data.
 * @param items - pointer to array of items
 * @param currentSize - size of the array of items
 */
void grow_item_array(Depot *info, Item **items, int currentSize) {
    // increase item size and realloc
    int totalSize = currentSize + 1;
    Item *temp = (Item *) realloc(*items, (totalSize * sizeof(Item)));

    // set item array
    if (temp == NULL) {
        return;
    } else {
        *items = temp;
    }

    info->totalItems = totalSize;
}

/**
 * Add item to the array of stored depot items
 * @param info - Depot struct holding related data.
 * @param new - Item struct to store.
 */
void item_add(Depot *info, Item *new) {
    // determine if item present in the array
    int found = 0;
    for (int i = 0; i < info->totalItems; i++) {
        if (strcmp(info->items[i].name, new->name) == 0) {
            found = 1;
            // if present, increase count
            info->items[i].count += new->count;
        }
    }

    // if not found, add a new entry
    if (found == 0) {
        grow_item_array(info, &info->items, info->totalItems);
        info->items[info->totalItems - 1] = *new;
    }

}

/**
 * Function to remove item from array of stored items
 * @param info - Depot struct holding related data.
 * @param remove - Item struct to remove
 */
void item_remove(Depot *info, Item *remove) {
    // determine if item present in the array
    int found = 0;
    for (int i = 0; i < info->totalItems; i++) {
        if (strcmp(info->items[i].name, remove->name) == 0) {
            found = 1;
            // if found, decrease the amout
            info->items[i].count -= remove->count;
        }
    }

    // if not found, set count to negative and add to list
    if (found == 0) {
        remove->count = remove->count * -1;
        item_add(info, remove);
    }
}

/**
 * Function to add neighbour to list of connections
 * @param list - pointer to list of connections
 * @param connection - connection struct to add to list
 * @param pos - pointer to integer for position to add connection
 * @param numElements - pointer to integer for length of connection array
 */
void add_attempt(Connection **list, Connection *connection, int *pos,
                 int *numElements) {
    /* increment size of list and store element */
    const int tempLength = *numElements * 2;
    Connection *temp = (Connection *) realloc(*list,
            tempLength * sizeof(Deferred));
    temp[*pos] = *connection;
    *pos += 1;
    *list = temp;
    *numElements = tempLength;
}

/**
 * Function to handle the recording of connections
 * @param info - Depot struct holding related data.
 * @param name - string representing name of server
 * @param port - integer representing port
 * @param in - file stream to the connection
 * @param out - file stream from the connection
 * @param status - integer (1 if confirmed via IM, 0 if not)
 */
void record_attempt(Depot *info, char *name, int port, FILE *in, FILE *out,
        int status) {
    // create struct and store values. Lock and unlock as required with mutex.
    Connection *server = malloc(sizeof(Connection));
    pthread_mutex_lock(&info->dataLock);
    server->name = name;
    server->addr = port;
    server->neighbourStatus = status;
    server->streamTo = in;
    server->streamFrom = out;

    // store neighbour, reallocate if required
    if (info->neighbourCount < info->neighbourLength - 1) {
        info->neighbours[info->neighbourCount] = *server;
        info->neighbourCount++;
    } else {
        add_attempt(&info->neighbours, server, &info->neighbourCount,
                &info->neighbourLength);
    }
    pthread_mutex_unlock(&info->dataLock);
}

/**
 * Function to handle the connection of the depot to other depots
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 */
void depot_connect(Depot *info, char *input) {
    // ensure sting format is ok
    input[strlen(input) - 1] = '\0';
    input += 7;
    if (input[0] != ':') {
        return;
    }
    input++;

    // get port from msg
//    char* port = malloc(sizeof(char) * strlen(input));
//    strcpy(port, input);
//    printf("port: %s, %lu\n", input, strlen(input));

    // check format of integer
    if (check_int(input) != 0) {
        return;
    }

    // prevent same port connection
    pthread_mutex_lock(&info->dataLock);
    for (int i = 0; i < info->neighbourCount; i++) {
        if (info->neighbours[i].addr == atoi(input)) {
            return;
        }
    }
    pthread_mutex_unlock(&info->dataLock);

    // connect to port.
    struct addrinfo *addressInfo = 0;
    struct addrinfo settings;
    memset(&settings, 0, sizeof(struct addrinfo));
    settings.ai_family = AF_INET; // IPv4 connection
    settings.ai_socktype = SOCK_STREAM; // connect peer to peer

    // attempt to parse address info
    if (getaddrinfo("localhost", input, &settings, &addressInfo)) {
        freeaddrinfo(addressInfo);
        return;   // could not work out the address
    }

    // create socket and connect to the port
    // use default protocol
    int fileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(fileDescriptor, (struct sockaddr *) addressInfo->ai_addr,
                sizeof(struct sockaddr))) {
        return;
    }

    // create file streams for communication to/from connection
    int dupFd = dup(fileDescriptor);
    FILE *to = fdopen(fileDescriptor, "w");
    FILE *from = fdopen(dupFd, "r");

    // spin up listening thread
    pthread_t tid;
    ThreadData *val = malloc(sizeof(ThreadData));
    val->depot = info;
    val->streamTo = to;
    val->streamFrom = from;
    val->channel = info->channel;
    val->signal = info->signal;

    pthread_create(&tid, 0, thread_listen, (void *) val);
}

/**
 * Function to check if illegal character / bad formatting in command
 * @param input - string of command to perform.
 * @param msg - ENUM representing type of command sent
 * @return 0 - formatting is error free
 *         -1 - bad formatting
 */
int check_illegal_char(char *input, Command msg) {
    int counter = 0;
    for (int i = 0; i < strlen(input); i++) {
        if (input[i] == ':') {
            counter++;
        }
        if (input[i] == '\r' || input[i] == ' ') {
            return -1;
        }
        if (input[i] == '\n' && ((i != strlen(input) - 1)
            || i != strlen(input))) {
            printf("%d %lu\n", i, strlen(input));
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

void depot_im(Depot *info, char *input, FILE *in, FILE *out) {
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
    char *portOrig = malloc(sizeof(char) * numberDigits);
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

    char *serverName = malloc(sizeof(char) * strlen(input));
    strcpy(serverName, input);
//    serverName[strlen(serverName) - 1] = '\0';

    // store connection as neighbour
//    for (int i = 0; i < info->neighbourCount; i++) {
//        if (info->neighbours[i].addr == port) {
//            info->neighbours[i].neighbourStatus = 1;
//            info->neighbours[i].name = serverName;
//        }
//    }
    record_attempt(info, serverName, port, in, out, 1);
}

int depot_deliver(Depot *info, char *input, int key) {
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
    char *quanOrig = malloc(sizeof(char) * numberDigits);
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

    char *itemName = malloc(sizeof(char) * strlen(input));
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
//        printf("in: %s %d\n", itemName, quantity);
        item_add(info, new);
    } else {
        // defer action
//        printf("def in: %s %d <%d>\n", itemName, quantity, key);
        Deferred *cmd = malloc(sizeof(Deferred));
        cmd->key = key;
        cmd->command = DELIVER;
        cmd->item = new;
        add_deferred(&info->deferred, &info->defLength, &info->defCount, cmd);

    }

    //todo DEBUG REMOVE
//    for (int i = 0; i < info->totalItems; i++) {
//        printf("%s:%d\n", info->items[i].name, info->items[i].count);
//    }
    return 0;

}

int depot_withdraw(Depot *info, char *input, int key) {
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
    char *quanOrig = malloc(sizeof(char) * numberDigits);
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

    char *itemName = malloc(sizeof(char) * strlen(input));
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
//    for (int i = 0; i < info->totalItems; i++) {
//        printf("%s:%d\n", info->items[i].name, info->items[i].count);
//    }
    return 0;
}

int depot_transfer(Depot *info, char *input, int key) {
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
    char *quanOrig = malloc(sizeof(char) * numberDigits);
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
    char *itemName = malloc(sizeof(char) * itemLength);
    strncpy(itemName, input, itemLength);
    input += itemLength;

    if (input[0] != ':') {
        return 0;
    }
    input++;

    char *serverName = malloc(sizeof(char) * strlen(input));
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
    char *keyOrig = malloc(sizeof(char) * numberDigits);
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
    char *order = malloc(sizeof(char) * numberLetters);
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
            printf("%u <%s>\n", info->neighbours[i].addr,
                    info->neighbours[i].name);
        }
    }

    printf("Deferred: ");
    printf("2:deliver, 3:widthdraw, 4:transfer\n");
    for (int i = 0; i < info->defCount; i++) {
        if (info->deferred[i].command == TRANSFER) {
            printf("<%d> %d %s:%d to %s\n", info->deferred[i].key,
                    info->deferred[i].command,
                   info->deferred[i].item->name, info->deferred[i].item->count,
                   info->deferred[i].location);
        } else {
            printf("<%d> %d %s:%d\n", info->deferred[i].key,
                    info->deferred[i].command,
                   info->deferred[i].item->name,
                   info->deferred[i].item->count);
        }
    }
    printf("-----------------------\n");

}

void process_input(Depot *info, char *input, FILE *in, FILE *out) {
    if (strncmp(input, "Connect", 7) == 0) {
        depot_connect(info, input);
    } else if (strncmp(input, "IM", 2) == 0) {
        depot_im(info, input, in, out);
    } else if (strncmp(input, "Deliver", 7) == 0) {
        depot_deliver(info, input, -1);
    } else if (strncmp(input, "Withdraw", 8) == 0) {
        depot_withdraw(info, input, -1);
    } else if (strncmp(input, "Transfer", 8) == 0) {
        depot_transfer(info, input, -1);
    } else if (strncmp(input, "Defer", 5) == 0) {
        defer(info, input);
    } else if (strncmp(input, "Execute", 7) == 0) {
//        printf("Execute \n");
    } else if (strncmp(input, "debug", 5) == 0) {
        debug(info); //todo REMOVE
    } else if (strncmp(input, "sig", 3) == 0) {
        sighup_print(info);
    }
}