#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <queue>

#include "ClientSocket.h"
#include "Worker.h"

#define THREADS 32

struct ThreadPool {
    std::vector<std::thread> threadPool;
    std::vector<Worker> workers;
    std::vector<ClientMessage> requests;

    std::array<ClientSocket, MAX_CONCURRENT_CLIENTS>& clients;
    std::queue<int>& clientQueue;
    std::mutex& clientQMut;
    std::condition_variable& clientQcond;

    std::atomic<bool> run;

    ThreadPool(std::array<ClientSocket, MAX_CONCURRENT_CLIENTS>& clients, std::queue<int>& clientQueue, std::mutex& clientQMut, std::condition_variable& clientQcond) 
        : threadPool(THREADS), workers(THREADS), requests(THREADS), clients(clients), clientQueue(clientQueue), clientQMut(clientQMut), clientQcond(clientQcond), run(true) {
            for (int i = 0; i < THREADS; ++i) {
                threadPool[i] = std::thread(&ThreadPool::threadFunc, this, i);
            }
        }

    void threadFunc(uint8_t i) {
        while (run) {
            int clientFd;
            {
                std::unique_lock<std::mutex> lock(clientQMut);
                clientQcond.wait(lock, [&] { return !clientQueue.empty(); });
                if (!run && clientQueue.empty()) return;

                clientFd = clientQueue.front();
                clientQueue.pop();
            }

            ClientSocket& client =  clients[clientFd];

            {
                std::unique_lock<std::mutex> lock(client.reqQMut);
                requests[i] = std::move(client.reqQ.front());
                client.reqQ.pop();
            }

            workers[i].processRequest(requests[i]);

            {
                std::lock_guard<std::mutex> lock(*client.clientQMut);
                client.addResponse(std::move(requests[i]));
            }

            {
                std::unique_lock<std::mutex> lock(client.reqQMut);
                if (!client.reqQ.empty()) {
                    std::unique_lock<std::mutex> qlock(clientQMut);
                    clientQueue.push(clientFd);
                    clientQcond.notify_one();
                } else {
                    client.isRegisteredInClientQueue.store(false);
                }
            }
        }
    }
};

#endif // THREADPOOL_H