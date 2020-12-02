#include "client.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "utils.hpp"

#define POLL_INTERVAL 150
#define BUFFER_SIZE 4096

using namespace std;

Client::Client(int _portno, const string _hostname, bool _interactive_mode)
{
    portno = _portno;
    hostname = _hostname;
    interactive_mode = _interactive_mode;
}

void Client::openServerConnection()
{
    struct sockaddr_in serv_addr;
    struct hostent *server;

    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0)
    {
        error("ERROR opening socket");
    }

    server = gethostbyname(hostname.c_str());
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);

    serv_addr.sin_port = htons(portno);
    if (connect(socketFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        error("ERROR connecting");
    }
}

void Client::startClient()
{

    openServerConnection();

    if (interactive_mode)
    {
        pollFds[0].fd = STDIN_FILENO;
        pollFds[0].events = POLLIN;
        pollFds[0].revents = 0;
        printf("Enter message to send to server: \n");
    }

    pollFds[1].fd = socketFd;
    pollFds[1].events = POLLIN;
    pollFds[1].revents = 0;

    handlePolling();
}

void Client::handlePolling()
{
    int n;
    char buffer[BUFFER_SIZE];

    while (keepPolling)
    {
        bzero(buffer, BUFFER_SIZE);

        if (poll(pollFds, 2, POLL_INTERVAL) == -1)
        {
            error("ERROR on polling");
        }

        if (interactive_mode && pollFds[0].revents && POLLIN)
        {
            fgets(buffer, BUFFER_SIZE, stdin);

            n = write(socketFd, buffer, strlen(buffer));
            if (n < 0)
            {
                error("ERROR writing to stdin");
            }
        }

        if (pollFds[1].revents && POLLIN)
        {
            bzero(buffer, BUFFER_SIZE);

            n = recv(socketFd, buffer, BUFFER_SIZE-1, MSG_DONTWAIT);
            if (n < 0)
            {
                error("ERROR reading from socket");
            }

            if (n > 0)
            {
                printf("Recieved: %s\n", buffer);

                if (string(buffer).substr(strlen(buffer) - 4, 4) == "done")
                {
                    keepPolling = false;
                }
            }
        }
    }

    closeClient();
}

void Client::closeClient()
{
    keepPolling = false;
    close(socketFd);
    printf("Closed connection with server.\n");
}

int main(int argc, char const *argv[])
{
    string hostname = "localhost";
    int portno = 9003;

    if (argc > 2)
    {
        hostname = argv[1];
        if (argc > 3) {
            portno = atoi(argv[2]);
        }
    }

    Client client(portno, hostname, true);

    client.startClient();

    return 0;
}
