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


// struct for the depot
typedef struct {
    char* name;
    Item* items;
    int totalItems;
    int server;
} Depot;

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

    // illegal char
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

void setup_listen(Depot *info) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(& hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;        // IPv6  for generic could use AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;  // Because we want to bind with it    
    int err;
    if (err = getaddrinfo("localhost", 0, &hints, &ai)) { // no particular port
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

    info->server = serv;
    return 0;
}

int listen(Depot info) {

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

    // listen 
    setup_listen(&info);
    listen(&info);

    
    return OK;

}


int main(int argc, char** argv) {
    if ((argc % 2) != 0 || argc < 2) {
        show_message(INCORRARGS);
    } else {
        return start_up(argc, argv);
    }
}
