#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>

/* Μέγιστος τυχαίος αριθμός για το "παιχνίδι" */
#define RAND_MAX 20

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char str[INET_ADDRSTRLEN];

    char buffer[1024];
    if (argc < 3)
    {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("Error opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        fprintf(stderr, "Error  no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
            (char *)&serv_addr.sin_addr.s_addr,
            server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("Error connecting");

    if (inet_ntop(AF_INET, &serv_addr.sin_addr, str, INET_ADDRSTRLEN) == NULL) {
        fprintf(stderr, "Could not convert byte to address\n");
        exit(1);
    }
    srand(getpid());    // Καθορισμός του random seed
    int keep_running = 1;
    for (;;) {
        printf("%s_> ", str);
        bzero(buffer, 1024);
        fgets(buffer, 1023, stdin);
        if (strcmp("END\n", buffer) == 0) {
            /* Εντοπίστηκε END. Πρώτα ειδοποιούμε τον server. */
            n = write(sockfd, buffer, strlen(buffer));
            /* Τώρα το παιχνίδι. Διαβάζουμε πόσες σωστές είχαμε. */
            int correct_commands;
            n = read(sockfd, &correct_commands, sizeof(int));
            if (n < 0) {
                error("Error reading from socket");
                break;
            } else if (n == 0) {
                printf("Connection terminated on the server side.\n");
                break;
            }
            /* Διαλέγουμε και στέλνουμε correct_commands τυχαίους αριθμούς
               στο [0,20] */
            int answer = -1;
            for (int i = 0; i < correct_commands; i++) {
                answer = rand();
                n = write(sockfd, &answer, sizeof(int));
            }
            if (correct_commands == 0) {
                /* Καμία σωστή εντολή. Σίγουρη ήττα. Στέλνουμε invalid αριθμό
                   ώστε να χάσουμε (-1). */
                   n = write(sockfd, &answer, sizeof(int));
            }
            /* Περιμένουμε το αποτέλεσμα */
            bzero(buffer, 1024);
            n = read(sockfd, buffer, 1023);
            if (n < 0) {
                error("Error reading from socket");
            } else if (n == 0) {
                printf("Connection terminated on the server side.\n");
            } else {
                printf("%s\n", buffer);
            }
            keep_running = 0;
            break;
        } else if (*buffer == '\n') {
            continue;
        }
        n = write(sockfd, buffer, strlen(buffer));
        if (n < 0)
            error("Error writing to socket");
        bzero(buffer, 1024);
        n = read(sockfd, buffer, 1023);
        if (n < 0){
            error("Error reading from socket");
        } else if (n == 0) {
            /* Η σύνδεση τερματίστηκε στην πλευρά του server */
            keep_running = 0;
            printf("Connection terminated on the server side.\n");
            break;
        } else {
            printf("%s\n", buffer);
        }
        if (!keep_running) {
            printf("Closing.\n");
            break;
        }
    }
    close(sockfd);
    return 0;
}
