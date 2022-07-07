#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "helpers.h"
#include <iostream>
#include <vector>
#include <queue>
#include <list>
#include <string>
#include <iomanip>
#include <sstream>
#include <math.h>

using namespace std;

struct TCPclient
{
    char ID_CLIENT[11];
    int socketNr;
};

struct subscriberNoSF
{
    char topic[60];
    char ID_Client[11];
};

struct subscriberSF
{
    char topic[60];
    char ID_Client[11];
    bool connected;
    queue<message> q;
};

void usage(char *file)
{
    fprintf(stderr, "Usage: %s server_address server_port\n", file);
    exit(0);
}

vector<TCPclient> clientsConnected;
list<subscriberSF> listOfSubscribersSF;
list<subscriberNoSF> listOfSubscribersNoSF;

void sendMessage(message messageToSend)
{

    int socketNr;

    // send message to all subscribers with SF = 0
    for (auto it = listOfSubscribersNoSF.begin(); it != listOfSubscribersNoSF.end(); it++)
    {
        if (strcmp(it->topic, messageToSend.topic) == 0)
        {

            // find socket nr
            for (int i = 0; i < clientsConnected.size(); i++)
                if (strcmp(it->ID_Client, clientsConnected[i].ID_CLIENT) == 0)
                {
                    socketNr = clientsConnected[i].socketNr;

                    // send message
                    int n = send(socketNr, &messageToSend, sizeof(messageToSend), 0);
                    DIE(n < 0, "Can not send message to subscriber with SF 0");
                }
        }
    }

    // send message to all subscribers with SF = 1 or store in q
    for (auto it = listOfSubscribersSF.begin(); it != listOfSubscribersSF.end(); it++)
    {
        if (strcmp(it->topic, messageToSend.topic) == 0)
        {
            if (it->connected == 1)
            {

                // find socket nr
                for (int i = 0; i < clientsConnected.size(); i++)
                    if (strcmp(it->ID_Client, clientsConnected[i].ID_CLIENT) == 0)
                        socketNr = clientsConnected[i].socketNr;

                // send message
                int n = send(socketNr, &messageToSend, sizeof(messageToSend), 0);
                DIE(n < 0, "Can not send message to subscriber with SF 1");
            }
            else
            {
                it->q.push(messageToSend);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    // disable buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc < 2)
    {
        usage(argv[0]);
    }

    fd_set read_fds, tmp_fds;
    int fdmax;

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    // check if port is valid
    int port = atoi(argv[1]);
    DIE(port == 0, "Wrong port.");

    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons((uint16_t)port);

    int sockfdTCP, sockfdUDP;

    // open socket for TCP client
    sockfdTCP = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfdTCP < 0, "Cannot open TCP socket.");

    int ret = bind(sockfdTCP, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    DIE(ret < 0, "bind tcp");

    ret = listen(sockfdTCP, MAX_CLIENTS);
    DIE(ret < 0, "listen");

    // disable neagle
    // https://stackoverflow.com/questions/17842406/how-would-one-disable-nagles-algorithm-in-linux
    int flag = 1;
    int neagle = setsockopt(sockfdTCP, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
    DIE(neagle < 0, "Cannot disable neagle.");

    // add TCP client in read_fds
    FD_SET(sockfdTCP, &read_fds);

    // open socket for UDP client
    sockfdUDP = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(sockfdUDP < 0, "Cannot open UDP socket.");

    ret = bind(sockfdUDP, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "bind udp");

    // add UDP client in read_fds
    FD_SET(sockfdUDP, &read_fds);

    if (sockfdTCP > sockfdUDP)
        fdmax = sockfdTCP;
    else
        fdmax = sockfdUDP;

    while (1)
    {
        tmp_fds = read_fds;

        int rc = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(rc < 0, "select");

        for (int i = 1; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &tmp_fds))
            {
                if (i == sockfdTCP) // new TCP client
                {
                    clilen = sizeof(cli_addr);
                    int newsockfd = accept(sockfdTCP, (struct sockaddr *)&cli_addr, &clilen);
                    DIE(newsockfd < 0, "Accept from new tcp client.");

                    // the client must send the ID first
                    char idBuffer[11];
                    memset(idBuffer, 0, 11);
                    int n = recv(newsockfd, idBuffer, sizeof(idBuffer), 0);
                    DIE(n < 0, "ID not sent");

                    // check if the id is already used
                    int isUsed = 0;

                    for (int i = 0; i < clientsConnected.size(); i++)
                    {
                        if (strcmp(clientsConnected[i].ID_CLIENT, idBuffer) == 0)
                        {
                            cout << "Client " << idBuffer << " already connected." << endl;

                            // send exit by type = -1
                            message aux;
                            aux.type = -1;

                            int n = send(newsockfd, &aux, sizeof(aux), 0);
                            DIE(n < 0, "can not send exit");

                            isUsed = 1;
                        }
                    }

                    if (isUsed == 1)
                        continue;
                    else
                    {
                        cout << "New client " << idBuffer << " connected from " << inet_ntoa(cli_addr.sin_addr) << ":" << ntohs(cli_addr.sin_port) << "." << endl;

                        // add socket to read_fds
                        FD_SET(newsockfd, &read_fds);
                        if (newsockfd > fdmax)
                        {
                            fdmax = newsockfd;
                        }

                        // add to clients' vector
                        TCPclient newClient;
                        memset(newClient.ID_CLIENT, 0, sizeof(newClient.ID_CLIENT));
                        strcpy(newClient.ID_CLIENT, idBuffer);
                        newClient.socketNr = newsockfd;
                        clientsConnected.push_back(newClient);

                        for (auto it = listOfSubscribersSF.begin(); it != listOfSubscribersSF.end(); it++)
                            if (strcmp(it->ID_Client, idBuffer) == 0)
                            {
                                it->connected = 1;
                                while (!it->q.empty())
                                {
                                    message aux;
                                    aux = it->q.front();
                                    int n = send(newsockfd, &aux, sizeof(aux), 0);
                                    DIE(n < 0, "can not send message stored");
                                    it->q.pop();
                                }
                            }
                    }
                }
                else if (i == sockfdUDP) // UDP client
                {
                    
                    char buffer[BUFLEN];
                    memset(buffer, 0, BUFLEN);
                    socklen_t len = sizeof(cli_addr);

                    int n = recvfrom(sockfdUDP, buffer, BUFLEN, 0, (struct sockaddr *)&cli_addr, &len);
                    DIE(n < 0, "Can not receive message from UDP client");

                    // get IP and port for the message
                    message aux;
                    aux.valid = 1;
                    inet_ntop(AF_INET, &(cli_addr.sin_addr), aux.IP_CLIENT_UDP, 16);
                    aux.port = ntohs(cli_addr.sin_port);

                    // check if message is too small
                    if (n < 50)
                    {
                        aux.valid = 0;
                        continue;
                    }

                    // get topic and type
                    memcpy(aux.topic, buffer, 50);
                    aux.type = (int)buffer[50];

                    // check if type is ok
                    if (aux.type < 0 || aux.type > 3)
                    {
                        aux.valid = 0;
                        continue;
                    }

                    // get content
                    switch (aux.type)
                    {
                    case 0:
                    {
                        uint32_t nrINT;
                        memcpy(&nrINT, buffer + 52, sizeof(uint32_t));
                        nrINT = ntohl(nrINT);

                        string nrToString;
                        nrToString = to_string(nrINT);

                        // check the sign
                        if (buffer[51] == 1)
                            nrToString = "-" + nrToString;

                        strcpy(aux.content, nrToString.c_str());

                        break;
                    }
                    case 1:
                    {
                        uint16_t nrSR;
                        memcpy(&nrSR, buffer + 51, sizeof(uint16_t));
                        nrSR = ntohs(nrSR);

                        float floatNr;
                        floatNr = 1.0 * nrSR / 100;

                        // set precision
                        // https://stackoverflow.com/questions/22515592/how-to-use-setprecision-in-c
                        stringstream stream;
                        stream << fixed << setprecision(2) << floatNr;

                        // copy info in content
                        string valueToCopyInContent = stream.str();
                        strcpy(aux.content, valueToCopyInContent.c_str());

                        break;
                    }
                    case 2:
                    {

                        uint32_t nrFL;
                        uint8_t power;

                        memcpy(&nrFL, buffer + 52, sizeof(uint32_t));
                        nrFL = ntohl(nrFL);
                        memcpy(&power, buffer + 52 + sizeof(uint32_t), sizeof(uint8_t));

                        string nrToString = to_string(nrFL);

                        //create number
                        if (power > nrToString.length())
                        {
                            nrToString.insert(0, power - nrToString.length() + 1, '0');
                            nrToString.insert(1, ".");
                        }
                        else if (power > 0)
                            nrToString.insert(nrToString.length() - power, ".");

                        //check the sign
                        if (buffer[51] == 1)
                            nrToString = "-" + nrToString;

                        strcpy(aux.content, nrToString.c_str());

                        break;
                    }
                    case 3:
                    {
                        memcpy(aux.content, buffer + 51, 1500);

                        break;
                    }
                    default:
                        aux.valid = 0;
                    }

                    if (aux.valid == 0)
                        continue;
                    else
                    {
                        sendMessage(aux);
                    }
                }
                else
                { // data from a client already connected

                    char buffer[BUFLEN], ID[11];
                    memset(buffer, 0, BUFLEN);
                    int n = recv(i, &buffer, sizeof(buffer), 0);
                    DIE(n < 0, "Did not receive from client already connected.");

                    // get ID and position in array of clients
                    int position;
                    for (int j = 0; j < clientsConnected.size(); j++)
                    {
                        if (clientsConnected[j].socketNr == i)
                        {
                            strcpy(ID, clientsConnected[j].ID_CLIENT);
                            position = j;
                        }
                    }

                    if (n == 0)
                    {
                        cout << "Client " << ID << " disconnected." << endl;
                        clientsConnected.erase(clientsConnected.begin() + position);

                        // change for all the subscription with SF connected to false
                        for (auto it = listOfSubscribersSF.begin(); it != listOfSubscribersSF.end(); it++)
                            if (strcmp(it->ID_Client, ID) == 0)
                                it->connected = 0;

                        close(i);
                        FD_CLR(i, &read_fds);
                    }
                    else if (strncmp(buffer, "subscribe", 9) == 0)
                    {
                        char topic[50], sbscr[10];
                        int SF;

                        sscanf(buffer, "%s %s %d", sbscr, topic, &SF);
                        char idAux[11];
                        strcpy(idAux, ID);

                        if (SF == 1)
                        {
                            subscriberSF aux;
                            strcpy(aux.ID_Client, idAux);
                            strcpy(aux.topic, topic);
                            aux.connected = 1;

                            listOfSubscribersSF.push_back(aux);
                        }
                        else
                        {
                            subscriberNoSF aux;
                            strcpy(aux.topic, topic);
                            strcpy(aux.ID_Client, ID);

                            listOfSubscribersNoSF.push_back(aux);
                        }
                    }
                    else if (strncmp(buffer, "unsubscribe", 11) == 0)
                    {

                        char topic[50];

                        char *token;
                        token = strtok(buffer, " ");
                        token = strtok(NULL, " ");
                        strcpy(topic, token);
                        topic[strlen(topic) - 1] = '\0';

                        // remove subscriber from lists
                        for (auto it = listOfSubscribersSF.begin(); it != listOfSubscribersSF.end();)
                        {
                            if (strcmp(it->ID_Client, ID) == 0 && strcmp(it->topic, topic) == 0)
                                it = listOfSubscribersSF.erase(it);
                            else
                                it++;
                        }

                        for (auto it = listOfSubscribersNoSF.begin(); it != listOfSubscribersNoSF.end();)
                        {
                            if (strcmp(it->ID_Client, ID) == 0 && strcmp(it->topic, topic) == 0)
                                it = listOfSubscribersNoSF.erase(it);
                            else
                                it++;
                        }
                    }
                }
            }
        }

        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) // from stdin
        {
            // read from stdin
            char buffer[BUFLEN];
            memset(buffer, 0, sizeof(buffer));
            int n = read(0, buffer, sizeof(buffer) - 1);
            DIE(n < 0, "read");

            if (strncmp(buffer, "exit", 4) == 0)
            {
                for (int i = 0; i < clientsConnected.size(); i++)
                {
                    message aux;
                    aux.type = -1;

                    int n = send(clientsConnected[i].socketNr, &aux, sizeof(aux), 0);
                    DIE(n < 0, "can not send exit");

                    close(clientsConnected[i].socketNr);
                }

                // clear the clients' array
                clientsConnected.clear();
                break;
            }
        }
    }

    close(sockfdTCP);
    close(sockfdUDP);

    return 0;
}
