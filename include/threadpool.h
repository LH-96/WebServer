#pragma once
#include <thread>
#include <mutex>
#include <functional>
#include <queue>
#include <vector>
#include <condition_variable>
#include <unistd.h>

class threadPool {
public:
    explicit threadPool(int threadCount = 8)
    : isClose(false), threadVec(std::vector<std::thread>(threadCount)) {
        for (int i = 0; i < threadCount; i++) {
            threadVec.at(i) = std::thread(&threadPool::worker, this, i);
        }
    }
    ~threadPool() {
        isClose = true;
        cond.notify_all();
        for (auto i = threadVec.begin(); i != threadVec.end(); i++) {
            if ((*i).joinable()) {
                (*i).join();
            }
        }
    }

public:
    template<typename T>
    void addTask(T &&task) {
        {
            std::lock_guard<std::mutex> locker(this->mux);
            this->workQueue.emplace(task);
        }
        this->cond.notify_one();
    }

private:
    void worker(int i) {
        while (true) {
            std::unique_lock<std::mutex> locker(this->mux);
            if (!this->workQueue.empty()) {
                this->cond.wait(locker, [this]{return !this->workQueue.empty();});
                auto task = std::move(this->workQueue.front());
                this->workQueue.pop();
                locker.unlock();
                task();
            }
            else if (this->isClose) {
                break;
            }
            else {
                this->cond.wait(locker);
            }
        }
    }

private:
    std::mutex mux;
    std::condition_variable cond;
    bool isClose;
    std::vector<std::thread> threadVec;
    std::queue<std::function<void()>> workQueue;
};
