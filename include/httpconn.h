#pragma once
#include <sys/unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <cstdarg>
#include <memory>
#include <atomic>

class httpConn {
public:
    static const char* rootPath;

    static const int maxPathLen = 200;

    static const int maxUrlLen = 100;

    static const int maxBuffSize = BUFSIZ;

    static std::atomic_int connNum;

    enum METHOD {GET = 0, POST};

    enum CONNECTION {CLOSE = 0, KEEPALIVE};

    enum MAINSTATUS {CHECK_REQUEST_LINE = 0, CHECK_REQUEST_HEADER, CHECK_REQUEST_CONTENT};

    enum SUBSTATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};

    enum HTTPCODE {NO_REQUEST = 0, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FILE_REQUEST, FORBIDDEN_REQUEST};

public:
    httpConn()
    :   parserRecord(std::make_shared<record>()),
        efd(0), cfd(0), readIndx(0), curReadIndx(0), 
        curLineBegin(0), writeIndx(0), curWriteIndx(0),
        ivCount(0), sendBytes(0) {
        memset(readBuffer, '\0', maxBuffSize);
        memset(writeBuffer, '\0', maxBuffSize);
    }
    ~httpConn() = default;

    httpConn(const httpConn&) = delete;
    httpConn& operator= (const httpConn&) = delete;

    void init(int efd, int cfd);

private:
    void resetfd(EPOLL_EVENTS eventStatus);
    void resetConn();
    void readData();
    char* getLine();
    SUBSTATUS readLine();
    HTTPCODE parseLine(char* text);
    HTTPCODE parseHeader(char* text);
    HTTPCODE parseContent(char* text);
    HTTPCODE doRequest();
    HTTPCODE httpParser(); 
    bool addResponse(const char* format, ...);
    bool addLine(int status, const char* title);
    bool addContentLen(int contentLen);
    bool addContentType();
    bool addConn();
    bool addBlankLine();
    void addHeader(int contentLen);
    bool addContent(const char* content);
    bool mergeResponse(HTTPCODE ret);
    bool writeData();

public:
    void closeConn();
    void processConn();
    void processRead();
    void processWrite();

private:
    // 记录解析http的信息
    struct record {
        MAINSTATUS parserStatus;
        METHOD method;
        char requestPath[maxPathLen];
        char requestUrl[maxUrlLen];
        struct stat fileStat;
        char* fileMMAP;
        char httpProt[10];
        CONNECTION conn;
        int contentLen;
        char* content;
    };
    std::shared_ptr<record> parserRecord;

private:
    int efd;
    int cfd;
    char readBuffer[maxBuffSize];
    char writeBuffer[maxBuffSize];
    int readIndx; // 下一个开始读的位置
    int curReadIndx;  // 当前解析的位置
    int curLineBegin; // 当前解析的行的首部
    int writeIndx; // 下一个开始写的位置
    int curWriteIndx;  // 当前写到的位置
    struct iovec iv[2];
    int ivCount;
    int sendBytes;
};
