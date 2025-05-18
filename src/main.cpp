#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <arpa/inet.h>
#include <google/protobuf/arena.h>

#include <vector>
#include <unordered_map>
#include <array>
#include <queue>
#include <thread>
#include <condition_variable>

#include "ServerSocket.h"
#include "ThreadPool.h"

#define EPOLL_EVENT_LIMIT 256

int main() {
    std::array<ClientSocket, MAX_CONCURRENT_CLIENTS> clients;
    std::queue<int> clientQueue;
    std::condition_variable clientQCond;
    std::mutex clientQMut;
    int epoll;
    
    // Setup epoll
    if ((epoll = epoll_create1(0)) == -1) {
        err(EXIT_FAILURE, "Failed to create epoll instance");
    }
    
    // Create server socket
    ServerSocket server(epoll, clients);

    // Prepare client sockets
    for (int i = 0; i != MAX_CONCURRENT_CLIENTS; i++) {
        clients[i].initializeSocket(i, epoll, &clientQueue, &clientQMut, &clientQCond);
    }

    // Setup threadPool
    ThreadPool tp(clients, clientQueue, clientQMut, clientQCond);

    // Epoll loop
    server.acceptConnections();
    struct epoll_event events[EPOLL_EVENT_LIMIT] {};
    while (true) {
        int n = epoll_wait(epoll, events, EPOLL_EVENT_LIMIT, -1);
        if (n == -1) { err(EXIT_FAILURE, "Epoll loop returned error"); }

        for (int i = 0; i != n; i++) {
            int fd = events[i].data.fd;
            uint ev = events[i].events;

            if (fd == server.fd) {
                server.processEpollin();
            } else {
                if (ev & EPOLLIN) {
                    clients[fd].readFromSocket();
                }

                if (ev & EPOLLOUT) {
                    clients[fd].writeToSocket();
                }
            }
        }
    }
}