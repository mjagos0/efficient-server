#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <queue>

#include "ClientSocket.h"
#include "Worker.h"

#define THREADS 2

struct ThreadPool {
    std::vector<std::thread> threadPool;
    std::vector<Worker> workers;
    std::vector<ClientMessage> requests;

    std::array<ClientSocket, MAX_CONCURRENT_CLIENTS>& clients;
    std::queue<ClientMessage>& reqQ;
    std::mutex& reqQMut;
    std::condition_variable& reqQCond;

    std::atomic<bool> run;

    ThreadPool(std::array<ClientSocket, MAX_CONCURRENT_CLIENTS>& clients, std::queue<ClientMessage>& reqQ, std::mutex& reqQMut, std::condition_variable& reqQCond) 
        : threadPool(THREADS), workers(THREADS), requests(THREADS), clients(clients), reqQ(reqQ), reqQMut(reqQMut), reqQCond(reqQCond), run(true) {
            for (int i = 0; i < THREADS; ++i) {
                threadPool[i] = std::thread(&ThreadPool::threadFunc, this, i);
            }
        }

    void threadFunc(uint8_t i) {
        while (run) {
            {
                std::unique_lock<std::mutex> lock(reqQMut);
                reqQCond.wait(lock, [&] { return !reqQ.empty(); });
                if (!run && reqQ.empty()) return;

                requests[i] = std::move(reqQ.front());
                reqQ.pop();
            }

            workers[i].processRequest(requests[i]);

            {
                std::lock_guard<std::mutex> lock(*clients[requests[i].clientFd].reqQMut);
                clients[requests[i].clientFd].addResponse(std::move(requests[i]));
            }
        }
    }
};

#endif // THREADPOOL_H