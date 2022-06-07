#pragma once
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <iostream>
#include <strings.h>
#include <string.h>
#include <sys/epoll.h>

#define MAX_EVENT_NUMBER 1024
#define MAX_BUFF_SIZE   BUFSIZ

class webserver {
public:
    // 构造、析构。。。
    webserver() = default;
    webserver(std::string ip, std::string port, std::string trigger)
    : ip(ip), port(port), trigger(trigger) {}
public:
    // public成员func
    void run();
private:
    // private成员func
    int initListenfd();
    void setNonblocking(int fd);
    void addfd(const int &efd, int fd);
    void readData(char *buf, int cfd);
    void buildConn(const int &efd, int listenfd);
    void epollHandler(const epoll_event *events, const int &eventsLen, 
                      const int &efd, const int &listenfd);
private:
    // private变量
    std::string ip, port, trigger;
};
