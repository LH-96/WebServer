#pragma once
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>
#include <iostream>
#include <cstring>

#include "threadpool.h"
#include "httpconn.h"
#include "timer.h"
#include "log.h"

extern const int maxEventNumber;
extern const int maxHttpConn;

class webserver {
public:
    // 构造、析构。。。
    explicit webserver(const char *ip, const char *port, int threadCount = 8,
                       int timeout = 30000, int isCloseLog = 1, int logType = 1, 
                       int logMaxBuf = BUFSIZ, int logMaxLine = 800000)
    : ip(ip), port(port), pool(std::make_unique<threadPool>(threadCount)), timeout(timeout),
        clients(maxHttpConn), timer(std::make_unique<heapTimer>(isCloseLog)),
        closeLog(isCloseLog), logType(logType), logMaxBufSiz(logMaxBuf), logMaxLines(logMaxLine)
    {}

    ~webserver() = default;

    webserver(const webserver&) = delete;
    webserver& operator= (const webserver&) = delete;

public:
    // public成员func
    void run();

private:
    // 初始化一个备用fd，当进程打开的fd达到上限时，用这个fd接受连接再马上关闭
    int backupfd = open("/dev/null", O_RDONLY | O_CLOEXEC);

    void logInit();
    int initListenfd();
    void setNonblocking(int fd);
    void addfd(int efd, int fd, bool isOneshot);
    void buildConn(int efd, int listenfd);
    void epollHandler(const epoll_event *events, const int &eventsLen, 
                      int efd, int listenfd);
                    
private:
    // private变量
    const char *ip, *port;
    int timeout;
    int closeLog;
    int logType;
    int logMaxBufSiz;
    int logMaxLines;
    std::unique_ptr<threadPool> pool;
    std::vector<httpConn> clients;
    std::unique_ptr<heapTimer> timer;
};
