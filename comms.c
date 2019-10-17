#include <pthread.h>
#include "2310depot.h"
#include "comms.h"
#include "channel.h"
#include <ctype.h>

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
void add_connection(Connection **list, Connection *connection, int *pos,
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
void record_neighbour(Depot *info, char *name, int port, FILE *in, FILE *out,
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
        add_connection(&info->neighbours, server, &info->neighbourCount,
                &info->neighbourLength);
    }
    pthread_mutex_unlock(&info->dataLock);
}

/**
 * Function to spin up a listening thread for a port
 * @param info - Depot struct to contain info
 * @param fileDescriptor - FD opened for the socket
 */
void spin_listening_thread(Depot *info, int fileDescriptor) {
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
 * Function to handle the connection of the depot to other depots
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 */
void depot_connect(Depot *info, char *input) {
    // ensure sting format is ok
    strtok(input, "\n"); // remove extra newlines
    input += 7; // remove CONNECT part of input
    if (input[0] != ':') {
        return;
    }
    input++;

    // check format of port & ensure no duplicate connections
    if (check_int(input) != 0) {
        return;
    }
    int portInt = atoi(input);
    if (portInt == info->listeningPort) {
        return; // prevent connection to self
    }
    pthread_mutex_lock(&info->dataLock);
    for (int i = 0; i < info->neighbourCount; i++) {
        if (info->neighbours[i].addr == atoi(input)) {
            return; // prevent connection to neighbour twice
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

    // create listening thread
    spin_listening_thread(info, fileDescriptor);
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
    // loop over all characters and check formatting
    for (int i = 0; i < strlen(input); i++) {
        if (input[i] == ':') {
            counter++;
        }
        if (input[i] == '\r' || input[i] == ' ') {
            return -1;
        }
        if (i < strlen(input) - 2) { //todo ok?
            if (input[i] == '\n') {
                return -1;
            }
        }
    }

    /* check for appropraite number of ':' symbols depending on message type */
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
    if (msg == DEFD || msg == DEFW) { // deferred deliver / withdraw
        if (counter != 4) {
            return -1;
        }
    }
    if (msg == DEFT) { // deferred transfer
        if (counter != 5) {
            return -1;
        }
    }

    return 0;
}

/**
 * Function to handle the reception of the IM message
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 * @param in - File stream into the server
 * @param out - File stream out of the server
 * return - 0 : successful connection
 *          -1 : bad IM, disconnect
 */
int depot_im(Depot *info, char *input, FILE *in, FILE *out) {
    strtok(input, "\n"); // remove extra newlines
    int checked = check_illegal_char(input, IM);
    if (checked != 0) {
        return -1;
    }

    /* check formatting of the message */
    input += 2; // remove IM part
    if (input[0] != ':') {
        return -1; // check ':' placement
    }
    input++;

    int numberDigits = 0;
    while (input[numberDigits] != ':') {
        numberDigits++; // count the number of digits in the port
    }
    char *portOrig = malloc(sizeof(char) * numberDigits);
    strncpy(portOrig, input, numberDigits);
    for (int i = 0; i < numberDigits; i++) {
        if (!isdigit(portOrig[i])) { // check that port is in fact a number
            return -1;
        }
    }
    int port = atoi(portOrig); // convert port to int
    if (port < 0 || port > 65535) { // prevent illegal ports
        return -1;
    }

    // check port not already present
    pthread_mutex_lock(&info->dataLock);
    for (int i = 0; i < info->neighbourCount; i++) {
        if (info->neighbours[i].addr == atoi(input)) {
            return -1; // prevent connection to neighbour twice
        }
    }
    pthread_mutex_unlock(&info->dataLock);

    input += numberDigits;  // move to next part of message
    if (input[0] != ':') {
        return -1;
    }
    input++;

    // record what is last, the server name - record the connection.
    char *serverName = malloc(sizeof(char) * strlen(input));
    strcpy(serverName, input);
    record_neighbour(info, serverName, port, in, out, 1);
    return 0;
}

/**
 * Function to control the delivery
 * @param info - Depot struct holding related data.
 * @param inputOrig - string of command to perform.
 * @param itemName - string of item name
 * @param quantity - integer of quantity of item
 * @param key - deferral key
 */
void control_deliver(Depot *info, char *inputOrig, char *itemName,
        int quantity, int key) {
    // check quantity and item name for formatting & create item struct
    if (quantity <= 0) {
        return;
    }
    if (strlen(itemName) == 0) {
        return;
    }
    Item *new = malloc(sizeof(Item));
    new->name = itemName;
    new->count = quantity;

    if (key == -1) {
        // add item directly to stores (non defer)
        item_add(info, new);
    } else {
        // defer delivering of item
        Deferred *cmd = malloc(sizeof(Deferred));
        cmd->key = key;
        cmd->command = DELIVER;
        cmd->item = new;
        cmd->input = inputOrig;
        add_deferred(&info->deferred, &info->defLength, &info->defCount, cmd);
    }
}

/**
 * Function to handle the deliver message
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 * @param key - integer for key if deferring the message
 */
void depot_deliver(Depot *info, char *input, int key) {
    // save the original message string
    char *inputOrig = malloc(sizeof(char) * strlen(input));
    strcpy(inputOrig, input);
    if (key == -1) {
        strtok(input, "\n"); // remove extra newlines if new message
    }
    // check for illegal characters
    int checked = check_illegal_char(input, DELIVER);
    if (checked != 0) {
        return;
    }

    /* check message format */
    input += 7; // remove DELIVER part of message
    if (input[0] != ':') {
        return; // check for ':' symbol
    }
    input++;
    int numberDigits = 0;
    while (input[numberDigits] != ':') {
        numberDigits++;
    }
    // check quantity portion of message is an integer
    char *quanOrig = malloc(sizeof(char) * numberDigits);
    strncpy(quanOrig, input, numberDigits);
    for (int i = 0; i < numberDigits; i++) {
        if (!isdigit(quanOrig[i])) {
            return;
        }
    }
    int quantity = atoi(quanOrig);

    // move to next part of message & check format
    input += numberDigits;
    if (input[0] != ':') {
        return;
    }
    input++;

    // save the item's name (final part of message)
    char *itemName = malloc(sizeof(char) * strlen(input));
    strcpy(itemName, input);

    // continue delivery
    control_deliver(info, inputOrig, itemName, quantity, key);
}

/**
 * Function to control the delivery
 * @param info - Depot struct holding related data.
 * @param inputOrig - string of command to perform.
 * @param itemName - string of item name
 * @param quantity - integer of quantity of item
 * @param key - deferral key
 */
void control_withdraw(Depot *info, char *inputOrig, char *itemName,
        int quantity, int key) {
    // check format of quantity & item name
    if (quantity <= 0) {
        return;
    }
    if (strlen(itemName) == 0) {
        return;
    }
    Item *new = malloc(sizeof(Item));
    new->name = itemName;
    new->count = quantity;

    if (key == -1) {
        // remove item instantly
        item_remove(info, new);
    } else {
        // defer withdraw of item
        Deferred *cmd = malloc(sizeof(Deferred));
        cmd->key = key;
        cmd->command = WITHDRAW;
        cmd->item = new;
        cmd->input = inputOrig;
        add_deferred(&info->deferred, &info->defLength, &info->defCount, cmd);
    }
}

/**
 * Function to handle the withdrawing of an item.
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 * @param key - integer for key if deferring the message
 */
void depot_withdraw(Depot *info, char *input, int key) {
    // save original message
    char *inputOrig = malloc(sizeof(char) * strlen(input));
    strcpy(inputOrig, input);
    if (key == -1) {
        strtok(input, "\n"); // remove extra newlines if new message
    }

    // check presence of illegal characters
    int checked = check_illegal_char(input, WITHDRAW);
    if (checked != 0) {
        return;
    }
    input += 8; // remove the CONNECT part of message and check ':' symbol
    if (input[0] != ':') {
        return;
    }
    input++;

    // check the quantity of the item to withdraw
    int numberDigits = 0;
    while (input[numberDigits] != ':') {
        numberDigits++;
    }
    char *quanOrig = malloc(sizeof(char) * numberDigits);
    strncpy(quanOrig, input, numberDigits);
    for (int i = 0; i < numberDigits; i++) {
        if (!isdigit(quanOrig[i])) { // check that quantity is a number
            return;
        }
    }
    int quantity = atoi(quanOrig); // convert quantity from sting to int

    input += numberDigits; // move to the item part of the message
    if (input[0] != ':') {
        return; // check placement of ':' symbol
    }
    input++;
    char *itemName = malloc(sizeof(char) * strlen(input)); // save name
    strcpy(itemName, input);

    // continue withdraw
    control_withdraw(info, inputOrig, itemName, quantity, key);
}

/**
 * Function to control transfer of items between two depots.
 * @param info - Depot struct holding related data.
 * @param input - string of input message
 * @param inputOrig - string of command to perform.
 * @param itemName - string of item name
 * @param itemLength - integer length of name of item
 * @param quantity - integer of quantity of item
 * @param key - deferral key
 */
void control_transfer(Depot *info, char *input, char *inputOrig,
        char *itemName, int itemLength, int quantity, int key) {
    input += itemLength; // move to next section (remove item name from string)
    if (input[0] != ':') {
        return; // check positioning of ':' symbol
    }
    input++;
    // store the server name
    char *serverName = malloc(sizeof(char) * strlen(input));
    strcpy(serverName, input);

    // check quantity, item name and server name formatting.
    if (quantity <= 0) {
        return;
    } else if (strlen(itemName) == 0 || strlen(serverName) == 0) {
        return;
    }

    // check if depot present so delivery can occur
    FILE *stream = NULL;
    for (int i = 0; i < info->neighbourCount; i++) {
        if (info->neighbours[i].name != NULL) {
            if (strcmp(info->neighbours[i].name, serverName) == 0) {
                stream = info->neighbours[i].streamTo; // successfully found
                break;
            }
        }
    }
    if (stream == NULL) {
        return; // haven't found depot supplied in message
    }

    // remove from this depot, and send message to add to other depot.
    Item *item = malloc(sizeof(Item));
    item->name = itemName;
    item->count = quantity;
    if (key == -1) {
        // withdraw from this depot and add to other depot via Deliver message
        item_remove(info, item);
        fprintf(stream, "Deliver:%d:%s\n", item->count, item->name);
        fflush(stream);
    } else {
        // defer transferring of items
        Deferred *cmd = malloc(sizeof(Deferred));
        cmd->key = key;
        cmd->command = TRANSFER;
        cmd->item = item;
        cmd->location = serverName;
        cmd->input = inputOrig;
        add_deferred(&info->deferred, &info->defLength, &info->defCount, cmd);
    }
}

/**
 * Function to handle the transfer of items from one depot to another.
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 * @param key - integer for key if deferring the message
 * @return
 */
void depot_transfer(Depot *info, char *input, int key) {
    // save original string message
    char *inputOrig = malloc(sizeof(char) * strlen(input));
    strcpy(inputOrig, input);
    if (key == -1) {
        strtok(input, "\n"); // remove extra newlines if deferred message
    }

    // check presence of illegal characters
    int checked = check_illegal_char(input, TRANSFER);
    if (checked != 0) {
        return;
    }
    // move to next part of message (remove TRANSFER section)
    input += 8;
    if (input[0] != ':') {
        return; // check presence of ':' symbol
    }
    input++;

    // check number of digits for quantity of item to transfer
    int numberDigits = 0;
    while (input[numberDigits] != ':') {
        numberDigits++;
    }
    char *quanOrig = malloc(sizeof(char) * numberDigits);
    strncpy(quanOrig, input, numberDigits);
    for (int i = 0; i < numberDigits; i++) {
        if (!isdigit(quanOrig[i])) { // check if quantity is an integer
            return;
        }
    }
    int quantity = atoi(quanOrig); // convert string quantity to integer

    input += numberDigits; // remove quantity section of message
    if (input[0] != ':') {
        return; // check positioning of ':' symbol
    }
    input++;

    // record length of item name & store item name
    int itemLength = 0;
    while (input[itemLength] != ':') {
        itemLength++;
    }
    char *itemName = malloc(sizeof(char) * itemLength);
    strncpy(itemName, input, itemLength);

    control_transfer(info, input, inputOrig, itemName, itemLength,
            quantity, key);
}

/**
 * Function to handle the deferral of a message
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 */
void defer(Depot *info, char *input) {
    strtok(input, "\n"); // remove extra newlines
    // store original input string
    char *inputOrig = malloc(sizeof(char) * strlen(input));
    strcpy(inputOrig, input);

    input += 5; // remove starting portion of msg
    if (input[0] != ':') {
        return; // check formatting
    }
    input++;

    // get key from the message
    int numberDigits = 0;
    while (input[numberDigits] != ':') {
        numberDigits++;
    }
    char *keyOrig = malloc(sizeof(char) * numberDigits);
    strncpy(keyOrig, input, numberDigits);
    for (int i = 0; i < numberDigits; i++) {
        if (!isdigit(keyOrig[i])) {
            return; // check that key is an unsigned int
        }
    }
    int key = atoi(keyOrig); // convert string to integer

    // move to next part of the string (remove key section)
    input += numberDigits;
    if (input[0] != ':') {
        return; // check formatting of ':'
    }
    input++;

    // get message to perform (deliver, withdraw, transfer)
    int numberLetters = 0;
    while (input[numberLetters] != ':') {
        numberLetters++;
    }
    char *order = malloc(sizeof(char) * numberLetters);
    strncpy(order, input, numberLetters);

    // store details of the message with it's key
    if (strcmp(order, "Deliver") == 0) {
        defer_deliver(info, input, inputOrig, key);
    } else if (strcmp(order, "Withdraw") == 0) {
        defer_withdraw(info, input, inputOrig, key);
    } else if (strcmp(order, "Transfer") == 0) {
        defer_transfer(info, input, inputOrig, key);
    }
}

/**
 * Function to handle the deferral of the delivery message
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 * @param orig - original string message
 * @param key - integer for key if deferring the message
 */
void defer_deliver(Depot *info, char *input, char *orig, int key) {
    // check the original string for formatting issues
    int checked = check_illegal_char(orig, DEFD);
    if (checked != 0) {
        return;
    }

    // defer the delivery with it's key
    depot_deliver(info, input, key);
}

/**
 * Function to handle the deferral of the withdraw message
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 * @param orig - original string message
 * @param key - integer for key if deferring the message
 */
void defer_withdraw(Depot *info, char *input, char *orig, int key) {
    // check the original string for formatting issues
    int checked = check_illegal_char(orig, DEFW);
    if (checked != 0) {
        return;
    }

    // defer the delivery with it's key
    depot_withdraw(info, input, key);
}

/**
 * Function to handle the transfer of the withdraw message
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 * @param orig - original string message
 * @param key - integer for key if deferring the message
 */
void defer_transfer(Depot *info, char *input, char *orig, int key) {
    // check the original string for formatting issues
    int checked = check_illegal_char(orig, DEFT);
    if (checked != 0) {
        return;
    }

    // defer the delivery with it's key
    depot_transfer(info, input, key);
}

/**
 * Control execution of deferred messages
 * @param info - struct of Depot info
 * @param key - integer key to execute deferred messages with
 */
void control_execute(Depot *info, int key) {
    /* add commands for worker consumption */
    for (int i = 0; i < info->defCount; i++) {
        if (key == info->deferred[i].key) {
            // create message to send to worker thread down channel
            Message *message = malloc(sizeof(Message));
            char *messageInput = malloc(
                    (strlen(info->deferred[i].input) + 2) * sizeof(char));
            strncpy(messageInput, info->deferred[i].input,
                    strlen(info->deferred[i].input));
            strcat(messageInput, "\n");
            message->input = messageInput;

            bool output = false;
            while (!output) { // stop once message successfully written
                // properly lock & unlock mutex.
                pthread_mutex_lock(&info->channelLock);
                output = write_channel(info->channel, message);
                pthread_mutex_unlock(&info->channelLock);
            }
            // signal that a message is ready
            sem_post(info->signal);

        }
    }

    /* remove all message that have been run with given key */
    bool foundKey;
    do {
        foundKey = false;
        int i;
        // check if any messages have given key
        for (i = 0; i < info->defCount; i++) {
            if (info->deferred[i].key == key) {
                foundKey = true;
                break;
            }
        }

        if (foundKey == true) {
            // If command with matching key found in array
            if (i < info->defCount) {
                // reduce size of array and shift elements
                info->defCount = info->defCount - 1;
                for (int j = i; j < info->defCount; j++) {
                    info->deferred[j] = info->deferred[j + 1];
                }
            }
        }
    } while (foundKey == true); // continue until all msg with keys removed
}

/**
 * Function to execute all deferred message
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 */
void depot_execute(Depot *info, char *input) {
    strtok(input, "\n"); // remove extra newlines
    char *inputOrig = malloc(sizeof(char) * strlen(input));
    strcpy(inputOrig, input);

    input += 7; // remove starting portion of msg (EXECUTE)
    if (input[0] != ':') {
        return; // check formatting of ':' symbol
    }
    input++;

    // check key format and parse from string to int
    int checkKey = check_int(input);
    if (checkKey != 0) {
        return;
    }
    int key = atoi(input);

    // continue execution of deferred messages
    control_execute(info, key);
}

/**
 * Function to handle the processing of an input from a given connection
 * @param info - Depot struct holding related data.
 * @param input - string of command to perform.
 * @param in - File stream into the server
 * @param out - File stream out of the server
 * @param socket - integer representing file descriptor of socket
 */
void process_input(Depot *info, char *input, FILE *in, FILE *out, int socket) {
    if (strncmp(input, "Connect", 7) == 0) {
        // connect to depot
        depot_connect(info, input);
    } else if (strncmp(input, "IM", 2) == 0) {
        int imStatus = depot_im(info, input, in, out);
        if (imStatus != 0) {
            // bad IM, disconnect & ignore
            fclose(in);
            fclose(out);
            close(socket);
        }
    } else if (strncmp(input, "Deliver", 7) == 0) {
        // deliver items to depot
        depot_deliver(info, input, -1);
    } else if (strncmp(input, "Withdraw", 8) == 0) {
        // withdraw items from depot
        depot_withdraw(info, input, -1);
    } else if (strncmp(input, "Transfer", 8) == 0) {
        // transfer items between two IM'd depots
        depot_transfer(info, input, -1);
    } else if (strncmp(input, "Defer", 5) == 0) {
        // defer message for later use (represented by a key)
        defer(info, input);
    } else if (strncmp(input, "Execute", 7) == 0) {
        // execute deferred message with a given key
        depot_execute(info, input);
    }
}