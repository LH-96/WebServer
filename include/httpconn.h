#pragma once
#include <sys/unistd.h>
#include <sys/epoll.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

class httpConn {
public:
    static const int maxBuffSize = BUFSIZ;

    enum METHOD {GET = 0, POST};

    enum MAINSTATE {CHECK_REQUEST_LINE = 0, CHECK_REQUEST_HEADER, CHECK_REQUEST_CONTENT};

    enum SUBSTATE {LINE_OK = 0, LINE_BAD, LINE_OPEN};

    enum HTTPCODE {NO_REQUEST = 0, GET_REQUEST, BAD_REQUEST};

public:
    httpConn()
    : efd(0), cfd(0), curReadIndx(0), curWriteIndx(0) {
        memset(readBuffer, '\0', maxBuffSize);
        memset(writeBuffer, '\0', maxBuffSize);
    }
    ~httpConn() = default;

    httpConn(const httpConn&) = delete;
    httpConn& operator= (const httpConn&) = delete;

    void init(int efd, int cfd);

private:
    void resetfd();
    void readData();
    void parseLine();
    bool httpParser(); 
    bool writeData();

public:
    void processConn();

private:
    int efd;
    int cfd;
    char readBuffer[maxBuffSize];
    char writeBuffer[maxBuffSize];
    int curReadIndx;
    int curWriteIndx;
};
