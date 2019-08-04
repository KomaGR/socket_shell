#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Μέγιστος τυχαίος αριθμός για το "παιχνίδι" */
#define RAND_MAX 20

#define VEC_SIZE 30

/* Το pipe[0] είναι για read και το pipe[1]
    είναι για write */

#define PARENT_READ_FD  ( pipes[1][0] )
#define PARENT_WRITE_FD ( pipes[0][1] )

#define CHILD_READ_FD   ( pipes[0][0] )
#define CHILD_WRITE_FD  ( pipes[1][1] )

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/* Μετρητής σωστών εντολών για το "παιχνίδι".
   Λόγω του copy-on-write στο fork() κάθε διεργασία
   παίρνει την τιμή correct_commands της μητρικής διεργασίας
   για κάθε shell. Αυτό σημαίνει ότι αν η αρχική διεργασία του
   server δεν πειράξει αυτήν την τιμή, κάθε παιδί της θα ξεκινάει
   με 0 και θα έχει ξεχωριστό μετρητή.*/
int correct_commands = 0;
pid_t allfather = 0;

void intHandler(int sig)
{
    int status;
    pid_t pid;
    switch (sig) {
        case SIGINT:
            /* Όλες οι διεργασίες περιλαμβάνουν αυτόν τον handler επομένως
               γίνονται όλες wait() */
            exit(0);
            break;
        case SIGCHLD:
            while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
                if (WIFEXITED(status)) {
                    /* Η διεργασία παιδί τερματίζει κανονικά */
                    fprintf(stdout,
                            "Successfully reaped %d, with exit status %d\n",
                            pid, status);
                    if (status == 0 && allfather != getpid()) {
                        /* Μια επιτυχής εκτέλεση έχει exit status 0 */
                        /* Αλλάζουμε τον counter της διεργασίας που έλαβε
                           το SIGCHLD, αν αυτή δεν είναι η πρωταρχική. */
                        correct_commands++;
                        fprintf(stdout, "%d: Correct commands: %d\n", getpid(), correct_commands);
                    }
                }
            }
            break;
        default:
            fprintf(stderr, "Caught unexpected signal: %d\n", sig);
    }
}
void parse(char *vector[VEC_SIZE], char *line) {
    int i;
    char * pch;
    pch = strtok(line, " ");
    i = 0;
    while (pch != NULL) {
        vector[i] = pch;
        printf("%s\n", pch);
        pch = strtok(NULL, " ");
        i++;
    }
    printf("i = %d\n ", i);
    vector[i] = (char *) NULL;
    int k = 0;
    for(k = 0; k <= i; k++) {
        printf("\tvector %d = %s \n", k, vector[k]);
    }
}
int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[1024];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    char str[INET_ADDRSTRLEN];
    struct sigaction sa;
    sa.sa_handler = intHandler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1)
        perror("Error: cannot handle SIGINT");
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGCHLD");
    }

    if (argc < 2)
    {
        fprintf(stderr, "No port provided\n");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd, 5);

    clilen = sizeof(cli_addr);
    /* Δηλώνουμε το pid της πρωταρχικής διεργασίας ώστε να διατηρήσουμε
       τον μετρητή correct_commands στο 0 για αυτήν */
    allfather = getpid();
    for (;;) {
        printf("Waiting for connections...\n");
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");

        if (inet_ntop(AF_INET, &cli_addr.sin_addr, str, INET_ADDRSTRLEN) == NULL) {
            fprintf(stderr, "Could not convert byte to address\n");
            exit(1);
        }
        fprintf(stdout, "The client address is :%s\n", str);
        /* Εδώ κάνουμε fork() για να δημιουργήσουμε μια διεργασία-παιδί
           για τον συγκεκριμένο client. */
        int pid, status;
        pid = fork();
        if (pid < 0) {
            /* error */
            perror("fork");
            exit(1);
        } else if (pid != 0) {
            /* parent */
            printf("Spawned child proccess %d\n", pid);
            /*  Οι διεργασίες παιδία γίνονται wait() με το
                SIGCHLD */
        } else if (pid == 0) {
            /* Βρόγχος παιδιού client */
            char *vector[VEC_SIZE];
            pid_t child_pid = getpid();
            for (;;) {
                bzero(buffer, 1024);
                n = read(newsockfd, buffer, 1023);
                if (n < 0) error("ERROR reading from socket");
                if (n == 0) {
                    /* Έκλεισε το socket. Τερματίζεται η επικοινωνία.
                       Δεν γίνεται να παιχτεί το "παιχνίδι". */
                    printf("This is the end for %d.\n", child_pid);
                    break;
                }
                if (strcmp("END\n", buffer) == 0) {
                    /* Εντοπίστηκε "END". Τώρα το παιχνίδι. */
                    printf("Time to play.\n");
                    /* Στέλνουμε στον client πόσες σωστές εντολές εκτέλεσε. */
                    n = write(newsockfd, &correct_commands, sizeof(correct_commands));
                    if (n < 0) error("ERROR writing to socket");
                    /* Διαλέγουμε τυχαία έναν αριθμό στο [0,20] */
                    srand(getpid());    // Καθορισμός seed
                    int winning_no = rand();
                    /* Έχει 0 σωστές. Θα χάσει σίγουρα. Βάζουμε 1 για
                       να δεχτούμε την "λάθος" απάντησή του. */
                    if (correct_commands == 0) correct_commands = 1;
                    int answers[correct_commands];
                    int client_win = 0;
                    /* Περιμένουμε την επιλογή του */
                    n = read(newsockfd, answers, correct_commands*sizeof(int));
                    if (n <= 0) {
                        /* Έκλεισε το socket ή άλλο σφάλμα.
                           Τερματίζεται η επικοινωνία.
                           Δεν γίνεται να παιχτεί το "παιχνίδι". */
                        printf("This is the end for %d.\n", child_pid);
                        break;
                    }
                    for (int i = 0; i < correct_commands; i++) {
                        if (answers[i] == winning_no) {
                            /* Ο client νίκησε */
                            client_win = 1;
                            break;
                        }
                    }
                    if (client_win == 1) {
                        printf("Client wins\n");
                        n = write(newsockfd, "Congratulations! You won!\n", 26);
                    } else {
                        printf("Server wins\n");
                        n = write(newsockfd, "You just lost the game.\n", 24);
                    }
                    printf("This is the end for %d.\n", child_pid);
                    break;
                }

                /* Warn: execute remote command */
                printf("%d is forking to run command: %s\n", child_pid, buffer);

                /* Έλεγχος για pipe ('|') στην εντολή */
                buffer[strcspn(buffer, "\n")] = 0;
                parse(vector, buffer);
                char **cmd2 = NULL;
                int k = 0, pipe_found = 0;
                for (k = 0; vector[k] != NULL; k++) {
                  if (strcmp(vector[k], "|") == 0) {
                    pipe_found = 1;       // Σημαία pipe
                    /*  Αντικατάσταση του συμβόλου '|' με NULL.
                        Έτσι και οι δυο εντολές είναι στο vector και είναι
                        και οι δυο null terminated. Το cast σε char* είναι
                        απαραίτητο για το execvp(). (Το λέει στο man page) */
                    vector[k] = (char *) NULL;
                    cmd2 = &vector[k+1];  // Εδώ ξεκινάει η δεύτερη εντολή (cmd2)
                  }
                }

                switch (pipe_found) {
                  case 0: {
                    /* Περίπτωση χωρίς pipe: */
                    int pipes[2][2];
                    pipe(pipes[0]);
                    pipe(pipes[1]);
                    pid_t exec_pid;
                    int count = 0;
                    char buff2[1024];
                    bzero(buff2, 1024);
                    if ((exec_pid = fork()) == -1) {
                      perror("Error forking for execvp()");
                    } else if (exec_pid != 0) {
                      /* exec parent */
                      /* Κλείνουμε file descriptors που δεν χρειάζονται από
                         αυτή τη διεργασία. */
                      close(CHILD_READ_FD);
                      close(CHILD_WRITE_FD);
                      printf("%d: Spawned proccess %d to execvp\n", getpid(), exec_pid);
                      count = read(PARENT_READ_FD, buff2, 1023);
                      if (count >= 0) {
                        buff2[count] = 0;
                        printf("%d: Command output %s\n", getpid(), buff2);
                      } else {
                        printf("IO Error\n");
                      }
                      /* Αποστολή αποτελέσματος στον client */
                      n = write(newsockfd, buff2, count+1);
                    } else {
                      /* exec child */
                      printf("%d: Running command: %s\n", getpid(), buffer);
                      // buffer[strcspn(buffer, "\n")] = 0;
                      // parse(vector, buffer);

                      /*  Αλλάζουμε τα stdin, stdout και stderr του child. */
                      dup2(CHILD_READ_FD, STDIN_FILENO);
                      dup2(CHILD_WRITE_FD, STDOUT_FILENO);
                      /* Χρειάζεται να αλλάξουμε και το stderr ώστε να
                         στέλνουμε τα σφάλματα στον client */
                      dup2(CHILD_WRITE_FD, STDERR_FILENO);
                      /* Κλείνουμε τα file descriptors που δεν είναι αναγκαία
                         από εδώ και πέρα. Επίσης και για λόγους ασφάλειας (
                         το νέο πρόγραμμα να μην έχει πρόσβαση) */
                      close(CHILD_READ_FD);
                      close(CHILD_WRITE_FD);
                      close(PARENT_READ_FD);
                      close(PARENT_WRITE_FD);
                      execvp(vector[0], vector);
                      perror(vector[0]);
                      exit(1);
                    }
                  }
                  break;
                  case 1: {
                      /* Περίπτωση με pipe: */
                      int pipes[2][2];
                      pipe(pipes[1]);   // Για την προσομοίωση του cmd1->|->cmd2
                      pipe(pipes[0]);   // Για να πάρουμε το output
                      pid_t exec_pid1, exec_pid2;
                      int count1 = 0, count2 = 0;
                      char buff1[1024], buff2[1024];
                      bzero(buff1, 1024);   // ενδιάμεσο buffer
                      bzero(buff2, 1024);   // output buffer

                      if ((exec_pid1 = fork()) == -1) {
                          perror("Error forking for execvp() first child");
                      } else if (exec_pid1 != 0) {
                          /* grand-parent */
                          close(pipes[0][1]);
                          close(pipes[1][0]);
                          close(pipes[1][1]);
                          printf("%d: Spawned proccess %d to execvp\n", getpid(), exec_pid1);
                          /* Διαβάζουμε το output του grand-child από το
                             pipe[0][0] */
                          count1 = read(pipes[0][0], buff2, 1023);

                          if (count1 >= 0) {
                            buff2[count1] = 0;
                            printf("%d: Command output %s\n", getpid(), buff2);
                          } else {
                            printf("IO Error\n");
                          }

                          n = write(newsockfd, buff2, count1);

                      } else if (exec_pid1 == 0) {
                          /* parent */
                          printf("%d: I will fork to run pipe.\n", getpid());
                          if ((exec_pid2 = fork()) == -1) {
                              perror("Error forking for execvp() second child");
                          } else if (exec_pid2 != 0) {
                              /* parent */
                              printf("%d: Spawned proccess %d to execvp\n", getpid(), exec_pid2);
                              /* Διάβασμα εισόδου από το pipes[1][0] */
                              dup2(pipes[1][0], STDIN_FILENO);
                              /* Γράψιμο εξόδου στο pipes[0][1] */
                              dup2(pipes[0][1], STDOUT_FILENO);
                              /* Χρειάζεται να αλλάξουμε και το stderr ώστε να
                                 στέλνουμε τα σφάλματα στον client */
                              dup2(pipes[0][1], STDERR_FILENO);
                              /* Κλείνουμε τα μη αναγκαία pipes */
                              close(pipes[1][1]);
                              close(pipes[0][0]);
                              /* Εκτέλεση δεύτερης εντολής */
                              execvp(cmd2[0], cmd2);
                              perror(cmd2[0]);
                              exit(1);
                          } else if (exec_pid2 == 0) {
                              /* child */
                              /* Γράψιμο στο pipes[1][1] */
                              dup2(pipes[1][1], STDOUT_FILENO);
                              /* Χρειάζεται να αλλάξουμε και το stderr ώστε να
                                 στέλνουμε τα σφάλματα στον client */
                              dup2(pipes[1][1], STDERR_FILENO);
                              /* Κλείνουμε τα μη αναγκαία pipes */
                              close(pipes[0][0]);
                              close(pipes[0][1]);
                              close(pipes[1][0]);
                              /* Εκτέλεση πρώτης εντολής */
                              execvp(vector[0], vector);
                              perror(vector[0]);
                              exit(1);
                          }
                      }
                  }
                }

            }
            printf("Closing socket\n");
            /* Κλείσιμο του socket του συγκεκριμένου client */
            close(newsockfd);
            exit(0);
        }
    }
    close(sockfd);
    return 0;
}
