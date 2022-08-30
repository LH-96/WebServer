#pragma once
#include <time.h>
#include <arpa/inet.h> 
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <functional> 
#include <chrono>
#include "log.h"

typedef std::function<void()> CALLBACK;
typedef std::chrono::high_resolution_clock CLOCK;
typedef std::chrono::milliseconds MS;
typedef CLOCK::time_point TIMEPOINT;

struct timeNode {
    int id;
    TIMEPOINT expires;
    CALLBACK cb;
    bool operator<(const timeNode& t) {
        return expires < t.expires;
    }
};

class heapTimer {
public:
    heapTimer(int isCloseLog = 1)
    :heapSize(0), closeLog(isCloseLog) { 
        heap.reserve(1024);
        heap.push_back(timeNode{0, TIMEPOINT{}, nullptr});
    }

    ~heapTimer() = default;
    
public:
    void addTimer(int id, int timeOut, const CALLBACK& cb);
    void adjustTimer(int id, int newExpires);
    int getNextTick();

private:
    void tick();
    void delTop(size_t i);
    void swapNode(size_t i, size_t j);

private:
    size_t heapSize;
    std::vector<timeNode> heap;
    std::unordered_map<int, size_t> idMap;
    int closeLog;
};
