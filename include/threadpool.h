#pragma once
#include <thread>
#include <mutex>
#include <functional>
#include <queue>
#include <vector>
#include <condition_variable>
#include <unistd.h>

/**
 * @brief 线程池
 * 
 */
class threadPool {
public:
    explicit threadPool(int threadCount = 8)
    : isClose(false), threadVec(std::vector<std::thread>(threadCount)) {
        for (int i = 0; i < threadCount; i++) {
            threadVec.at(i) = std::thread(&threadPool::worker, this);
            // printf("init\n");
        }
    }
    ~threadPool() {
        isClose = true;
        cond.notify_all();
        for (auto i = threadVec.begin(); i != threadVec.end(); i++) {
            if ((*i).joinable()) {
                (*i).join();
                // printf("close\n");
            }
        }
    }

    threadPool(const threadPool&) = delete;
    threadPool& operator=(const threadPool&) = delete;

public:
    /**
     * @brief 添加任务到队列中，并唤醒一个线程
     * 
     * @tparam T 任务类型，是一个函数
     * @param task 任务
     */
    template<typename T>
    void addTask(T &&task) {
        {
            std::lock_guard<std::mutex> locker(this->mux);
            this->workQueue.emplace(std::forward<T>(task));
        }
        this->cond.notify_one();
    }

private:
    /**
     * @brief 执行任务
     * 
     */
    void worker() {
        while (true) {
            std::unique_lock<std::mutex> locker(this->mux);
            if (!this->workQueue.empty()) {
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
