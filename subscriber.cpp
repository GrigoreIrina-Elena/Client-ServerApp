#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"
#include <iostream>

using namespace std;

void usage(char *file)
{
    fprintf(stderr, "Usage: %s server_address server_port\n", file);
    exit(0);
}

int main(int argc, char *argv[])
{
    // disable buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc < 4)
    {
        usage(argv[0]);
    }

    fd_set read_fds, tmp_fds;
    int fdmax;

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    // check if port is valid
    int port = atoi(argv[3]);
    DIE(port == 0, "Wrong port.");

    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)port);
    int ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "inet_aton");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    // disable neagle
    // https://stackoverflow.com/questions/17842406/how-would-one-disable-nagles-algorithm-in-linux
    int flag = 1;
    int neagle = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
    DIE(neagle < 0, "Cannot disable neagle.");

    ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "connect");

    // add client in read_fds
    FD_SET(sockfd, &read_fds);
    fdmax = sockfd;

    // send id to the server
    char id[11];
    strcpy(id, argv[1]);
    ret = send(sockfd, id, sizeof(id), 0);
    DIE(ret < 0, "cannot send id");

    while (1)
    {
        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        if (FD_ISSET(STDIN_FILENO, &tmp_fds))
        {
            // read from stdin
            char buffer[BUFLEN];
            memset(buffer, 0, sizeof(buffer));
            int n = read(0, buffer, sizeof(buffer) - 1);
            DIE(n < 0, "read");

            // received exit
            if (strncmp(buffer, "exit", 4) == 0)
            {
                break;
            }

            // received subscribe
            if (strncmp(buffer, "subscribe", 9) == 0)
            {
                n = send(sockfd, buffer, strlen(buffer), 0); // announce server
                DIE(n < 0, "failed to send subscribe");
                cout << "Subscribed to topic." << endl;
            }

            // received unsubscribe
            if (strncmp(buffer, "unsubscribe", 11) == 0)
            {
                n = send(sockfd, buffer, strlen(buffer), 0); // announce server
                DIE(n < 0, "failed to send unsubscribe");
                cout << "Unsubscribed from topic." << endl;
            }
        }

        if (FD_ISSET(sockfd, &tmp_fds))
        {
            // receive message
            message aux;
            int n = recv(sockfd, &aux, sizeof(aux), 0);
            DIE(n < 0, "can not receive");

            // if type is -1, exit
            if (aux.type == -1)
            {
                break;
            }

            // print message
            cout << aux.IP_CLIENT_UDP << ":" << aux.port << " - " << aux.topic << " - ";
            if (aux.type == 0)
            {
                cout << "INT"
                     << " - " << aux.content;
            }
            else if (aux.type == 1)
            {
                cout << "SHORT_REAL"
                     << " - " << aux.content;
            }
            else if (aux.type == 2)
            {
                cout << "FLOAT"
                     << " - " << aux.content;
            }
            else
            {
                cout << "STRING"
                     << " - " << aux.content;
            }
            cout << endl;
        }
    }

    close(sockfd);

    return 0;
}
