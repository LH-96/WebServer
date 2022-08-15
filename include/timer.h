#pragma once
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <chrono>
#include <functional>
#include <set>
#include <unordered_map>

typedef std::chrono::steady_clock CLOCK;
typedef std::chrono::milliseconds MS;
typedef CLOCK::time_point TIMEPOINT;

// 定时器节点
typedef struct node {
public:
    node() = default;
    node(int cfd, time_t expire, std::function<void()> func)
    : cfd(cfd), expire(expire), func(func) {}
    ~node() = default;

public:
    int cfd;
    time_t expire;
    std::function<void()> func;
}timeNode;

class timer {
public:
    timer() = default;
    timer(const timer&) = delete;
    timer& operator=(const timer&) = delete;
    ~timer() = default;

private:
    // 获取系统启动到当前时间
    static time_t getTime();

private:
    bool isTCPConn(int sock);

public:
    // 获取距离任务触发还需要的时间
    time_t timeToTrig();
    // 添加定时器节点
    void addTimer(int cfd, time_t expire, std::function<void()> func);
    //调整定时器节点
    void resetTimer(int cfd, time_t expireNew);
    // 判断节点是否需要执行任务(断开socket连接)，并删除节点
    bool checkTimer();

private:
    std::unordered_map<int, timeNode> fdToNode;
    std::set<timeNode, std::less<>> timeMap;
};
