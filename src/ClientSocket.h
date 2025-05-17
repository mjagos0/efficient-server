#ifndef CLIENTSOCKET_H
#define CLIENTSOCKET_H

#include <stdint.h>
#include <arpa/inet.h>
#include <condition_variable>

#include "ClientMessage.h"
#include "schema.pb.h"

#define READ_BUFFER_SIZE 31000
#define WRITE_BUFFER_SIZE 512
#define PROTO_LENGTH_PREFIX 4

struct ClientSocket {
    int fd;
    int epoll;
    std::queue<ClientMessage>* reqQ;
    std::mutex* reqQMut;
    std::queue<ClientMessage> socketRespQ;
    std::mutex respQMut;
    std::condition_variable* reqQCond;
    std::atomic<bool> isRegisteredForWrite;

    uint16_t activeRequests;

    uint8_t writeBuffer[WRITE_BUFFER_SIZE];
    uint16_t readBufferCursor;
    uint32_t readMessageSize;
    bool wantsToClose;

    uint8_t readBuffer[READ_BUFFER_SIZE];
    uint16_t writeBufferCursor;
    uint32_t writeMessageSize;

    ClientSocket(const int fd, const int epoll, std::queue<ClientMessage>* reqQ,
        std::mutex* reqQMut, std::condition_variable* reqQCond
    ) 
        : fd(fd), epoll(epoll), reqQ(reqQ), reqQMut(reqQMut), reqQCond(reqQCond),
        isRegisteredForWrite(false) { }

    ClientSocket()
    : fd(-1), epoll(-1), reqQ(nullptr), reqQMut(nullptr), reqQCond(nullptr),
      isRegisteredForWrite(false), activeRequests(0),
      readBufferCursor(0), readMessageSize(0), wantsToClose(false),
      writeBufferCursor(0), writeMessageSize(0) { }

    ~ClientSocket() {
        close(fd);
    }

    void initializeSocket(const int fd, const int epoll, std::queue<ClientMessage>* reqQ,
        std::mutex* reqQMut, std::condition_variable* reqQCond) {
            this->fd = fd;
            this->epoll = epoll;
            this->reqQ = reqQ;
            this->reqQMut = reqQMut;
            this->reqQCond = reqQCond;
    }

    void setUpSocket() {
        activeRequests = 0;
        readBufferCursor = 0;
        readMessageSize = 0;
        wantsToClose = false;
        writeBufferCursor = 0;
        writeMessageSize = 0;
    }

    void registerToEpoll() {
        struct epoll_event event {};
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = fd;

        if (epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event) == -1) {
            err(EXIT_FAILURE, "Failed to register client socket to Epoll");
        }
    }

    void tearDownSocket() const {
        std::cout << "Closing socket" << std::endl;

        if (epoll_ctl(epoll, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            err(EXIT_FAILURE, "Failed to remove client socket from epoll");
        }

        if (close(fd) == -1) {
            err(EXIT_FAILURE, "Failed to close client socket");
        }
    }

    void registerToWrite() {
        struct epoll_event event {};
        event.events = EPOLLIN | EPOLLET | EPOLLOUT;
        event.data.fd = fd;

        if (epoll_ctl(epoll, EPOLL_CTL_MOD, fd, &event) == -1) {
            err(EXIT_FAILURE, "Failed to register client socket to Epoll");
        }
    }

    void unregisterFromWrite() {
        struct epoll_event event {};
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = fd;

        if (epoll_ctl(epoll, EPOLL_CTL_MOD, fd, &event) == -1) {
            err(EXIT_FAILURE, "Failed to unregister client socket to Epoll");
        }
    }

    void addResponse(ClientMessage resp) {
        {
            std::unique_lock<std::mutex> lock(respQMut);
            socketRespQ.push(resp);
        }

        bool expected = false;
        if (isRegisteredForWrite.compare_exchange_strong(expected, true)) {
            registerToWrite();
        }
    }

    void readFromSocket() {
        while (true) {
            const int bytesRead = recv(fd, readBuffer + readBufferCursor, READ_BUFFER_SIZE - readBufferCursor, 0);
            if (bytesRead == -1)  {
                if (errno == EAGAIN) {
                    break;
                } else {
                    err(EXIT_FAILURE, "Failed to read from socket");
                }
            } else if (bytesRead == 0) {
                wantsToClose = true;
                return;
            }
            readBufferCursor += bytesRead;


            while ((readMessageSize == 0 && readBufferCursor >= PROTO_LENGTH_PREFIX) 
                || (readMessageSize != 0 && readBufferCursor >= readMessageSize + PROTO_LENGTH_PREFIX)) 
            {
                if (readMessageSize == 0 && readBufferCursor >= PROTO_LENGTH_PREFIX) {
                    uint32_t sizeBuffer;
                    memcpy(&sizeBuffer, readBuffer, PROTO_LENGTH_PREFIX);
                    readMessageSize = ntohl(sizeBuffer);

                    if (readMessageSize > READ_BUFFER_SIZE - PROTO_LENGTH_PREFIX) {
                        err(EXIT_FAILURE, "Client sent larger message than buffer can handle");
                    }
                }

                if (readBufferCursor >= readMessageSize + PROTO_LENGTH_PREFIX) {
                    ClientMessage request(fd);
                    request.request.resize(readMessageSize);
                    memcpy(request.request.data(), readBuffer + PROTO_LENGTH_PREFIX, readMessageSize);

                    {
                        std::unique_lock<std::mutex> lock(*reqQMut);
                        reqQ->push(std::move(request));
                    }
                    reqQCond->notify_one();
                    activeRequests++;

                    const uint16_t remaining = readBufferCursor - (PROTO_LENGTH_PREFIX + readMessageSize);
                    memmove(readBuffer, readBuffer + PROTO_LENGTH_PREFIX + readMessageSize, remaining);
                    readBufferCursor = remaining;
                    readMessageSize = 0;
                }
            }
        }
    }

    void writeToSocket() {
        while (true) {
            if (writeMessageSize == 0) {
                ClientMessage req;
                {
                    std::unique_lock<std::mutex> lock(respQMut);
                    if (socketRespQ.empty()) {
                        bool expected = true;
                        if (isRegisteredForWrite.compare_exchange_strong(expected, false)) {
                            unregisterFromWrite();
                        }
                        return;
                    } else {
                        req = std::move(socketRespQ.front());
                        socketRespQ.pop();
                    }
                }
  
                writeMessageSize = req.response.size();
                if (writeMessageSize > WRITE_BUFFER_SIZE) {
                    err(EXIT_FAILURE, "Response message size exceeded");
                }

                writeBufferCursor = 0;
                memcpy(writeBuffer, req.response.data(), writeMessageSize);
            }

            const int bytesWritten = send(fd, writeBuffer +  writeBufferCursor, writeMessageSize - writeBufferCursor, 0);
            
            if (bytesWritten == -1) {
                if (errno == EAGAIN) {
                    break;
                } else {
                    err(EXIT_FAILURE, "Failed to write to socket");
                }
            } else {
                writeBufferCursor += bytesWritten;
                if (writeBufferCursor == writeMessageSize) {
                    activeRequests--;
                    writeMessageSize = 0;

                    if (wantsToClose && activeRequests == 0) {
                        unregisterFromWrite();
                        tearDownSocket();
                        return;
                    }
                }
            }
        }
    }
};

#endif // CLIENTSOCKET_H