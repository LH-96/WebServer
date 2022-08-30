#pragma once
#include <sys/time.h>
#include <ctime>
#include <cstring>
#include <cstdarg>
#include <iostream>
#include <string>
#include "threadpool.h"

class log {
public:
    static log *getInstance() {
        static log instance;
        return &instance;
    }

    static void flushLogThread(std::string logStr, struct tm my_tm, long long curLine) {
        log::getInstance()->flush(logStr, my_tm, curLine);
    }

    bool init(const char *fileName, int isCloseLog, 
               int bufSize = BUFSIZ, int maxLines = 800000, int isAsync = 1);

    void writeLog(int level, const char *format, ...);

    void flush(std::string logStr, struct tm my_tm, long long curLine);

private:
    log() : closeLog(0), asyncLog(true), filePtr(nullptr), logBuffer(nullptr) {}
    ~log() {
        if (filePtr != nullptr)
            fclose(filePtr);
        if (logBuffer != nullptr)
            delete [] logBuffer;
    }

    log(const log&) = delete;
    log& operator=(const log&) = delete;

    void resetFile(struct tm my_tm);

private:
    char dirName[128]; //路径名
    char logName[128]; //log文件名
    int logMaxLines;  //日志最大行数
    int logBufferSize; //日志缓冲区大小
    long long logLinesCount;  //日志行数记录
    int today;        // 记录当前时间是那一天
    FILE *filePtr;         //打开log的文件指针
    char *logBuffer;    // log 缓冲区
    std::mutex mux;     // 缓冲区锁
    std::unique_ptr<threadPool> workThread; // 异步工作线程
    bool asyncLog;      // 同步标志
    int closeLog;       // 关闭日志
};

#define LOG_DEBUG(format, ...) if(0 == closeLog) {log::getInstance()->writeLog(0, format, ##__VA_ARGS__);}
#define LOG_INFO(format, ...) if(0 == closeLog) {log::getInstance()->writeLog(1, format, ##__VA_ARGS__);}
#define LOG_WARN(format, ...) if(0 == closeLog) {log::getInstance()->writeLog(2, format, ##__VA_ARGS__);}
#define LOG_ERROR(format, ...) if(0 == closeLog) {log::getInstance()->writeLog(3, format, ##__VA_ARGS__);}
