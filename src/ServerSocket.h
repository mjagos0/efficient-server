#ifndef SERVERSOCKET_H
#define SERVERSOCKET_H

#include <stdint.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>

#include "ClientSocket.h"

#define PORT 50088
#define MAX_CONCURRENT_CLIENTS 256

struct ServerSocket {
    std::array<ClientSocket, MAX_CONCURRENT_CLIENTS>& clients;
    const int epoll;
    int fd;
  
    ServerSocket(const int epoll, std::array<ClientSocket, MAX_CONCURRENT_CLIENTS>& clients) 
        : clients(clients), epoll(epoll) {
        if ((fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
            err(EXIT_FAILURE, "Failed to create server socket");
        }

        struct sockaddr_in saddr {};
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(PORT);
        saddr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
            err(EXIT_FAILURE, "Failed to bind server socket");
        }
    }

    ~ServerSocket() {
        close(fd);
    }

    void acceptConnections() {
        registerToEpoll();

        if (listen(fd, SOMAXCONN) == -1) {
           err(EXIT_FAILURE, "Server socket failed to listen");
        }
    }

    void registerToEpoll() {
        struct epoll_event ev {};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;

        if (epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &ev) == -1) {
            err(EXIT_FAILURE, "Server socket failed to register to Epoll");
        }
    }

    void closeServer() {
        if (close(fd) == -1) {
            err(EXIT_FAILURE, "Server socket failed to close");
        }
    }

    void processEpollin() {
        while (true) {
            int cfd = accept(fd, NULL, NULL);
            if (cfd == -1) {
                if (errno == EAGAIN) {
                    break;
                } else {
                    err(EXIT_FAILURE, "Failed to accept connection");
                }
            }

            int flags = fcntl(cfd, F_GETFL, 0);
            if (flags == -1 || fcntl(cfd, F_SETFL, flags | O_NONBLOCK) == -1) {
                err(EXIT_FAILURE, "Failed to set client socket to non-blocking");
            }

            if (cfd >= MAX_CONCURRENT_CLIENTS) {
                err(EXIT_FAILURE, "Max concurrent clients exceeded");
            }

            clients[cfd].setUpSocket();
            clients[cfd].registerToEpoll();
        }
    }
};

#endif // SERVERSOCKET_H