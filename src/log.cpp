#include "log.h"

bool log::init(const char *fileName, int isCloseLog, 
               int bufSize, int maxLines, int isAsync) {
    //异步
    if (isAsync == 1)
        workThread = std::make_unique<threadPool>(1);
    else
        asyncLog = false;
    
    closeLog = isCloseLog;
    logBufferSize = bufSize;
    logBuffer = new char[logBufferSize];
    memset(logBuffer, '\0', logBufferSize);
    logMaxLines = maxLines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(fileName, '/');
    char logFullName[256] = {0};
    if (p == nullptr) {
        snprintf(logFullName, 255, "%d_%02d_%02d_%s", 
                 my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, fileName);
    }
    else {
        strcpy(logName, p + 1);
        strncpy(dirName, fileName, p - fileName + 1);
        snprintf(logFullName, 255, "%s%d_%02d_%02d_%d_%s", 
                 dirName, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, 0, logName);
    }
    today = my_tm.tm_mday;
    filePtr = fopen(logFullName, "a");

    return filePtr != nullptr;
}

void log::resetFile(struct tm my_tm) {
    // 当前时间不是结构体存的时间 或 已经超出当前log文件记录最大行数，就关闭旧文件开启新文件。    
    char newLogFile[256] = {0};
    fflush(filePtr);
    fclose(filePtr);
    char tail[16] = {0};
    
    snprintf(tail, 16, "%d_%02d_%02d_", 
                my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
    if (today != my_tm.tm_mday) {
        snprintf(newLogFile, 255, "%s%s%d_%s", dirName, tail, 0, logName);
        today = my_tm.tm_mday;
        logLinesCount = 0;
    }
    else {
        snprintf(newLogFile, 255, "%s%s%lld_%s", 
                    dirName, tail, logLinesCount / logMaxLines, logName);
    }
    filePtr = fopen(newLogFile, "a");
}

void log::writeLog(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    char s[16] = {0};
    switch (level) {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    va_list valst;
    va_start(valst, format);

    std::string log_str;
    mux.lock();

    // log数量+1
    logLinesCount++;
    int curLine = logLinesCount;

    //写入的具体时间内容格式
    int n = snprintf(logBuffer, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    
    int m = vsnprintf(logBuffer + n, logBufferSize - 1, format, valst);
    logBuffer[n + m] = '\n';
    logBuffer[n + m + 1] = '\0';
    log_str = logBuffer;

    mux.unlock();

    if (asyncLog) {
        workThread->addTask(std::bind(&log::flushLogThread, log_str, my_tm, curLine));
    }
    else {
        mux.lock();
        fputs(log_str.c_str(), filePtr);
        fflush(filePtr);
        if (today != my_tm.tm_mday || logLinesCount % logMaxLines == 0)
            resetFile(my_tm);
        mux.unlock();
    }

    va_end(valst);
}

void log::flush(std::string logStr, struct tm my_tm, long long curLine)
{
    fputs(logStr.c_str(), filePtr);
    fflush(filePtr);
    if (today != my_tm.tm_mday || curLine % logMaxLines == 0)
        resetFile(my_tm);
}
