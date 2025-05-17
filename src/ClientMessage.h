#ifndef CLIENTMESSAGE_H
#define CLIENTMESSAGE_H

#include <vector>
#include <stdint.h>

struct ClientMessage {
    ClientMessage() { }
    ClientMessage(const int clientFd) : clientFd(clientFd) { }

    int clientFd;
    std::vector<uint8_t> request;
    std::vector<uint8_t> response;
};

#endif // CLIENTMESSAGE_H