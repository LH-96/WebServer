#include "httpconn.h"

int httpConn::connNum = 0;

/**
 * @brief 在建立一个http连接时，初始化一个http对象(全部参数初始化，防止fd被重用后，上次残留数据)
 * 
 * @param efd 
 * @param cfd 
 */
void httpConn::init(int efd, int cfd) {
    // 初始化http解析信息，默认get方法，短连接
    this->parserRecord->parserStatus = CHECK_REQUEST_LINE;
    this->parserRecord->method = GET;
    memset(this->parserRecord->RequestPath, '\0', maxPathLen);
    memset(this->parserRecord->httpProt, '\0', 10);
    this->parserRecord->conn = CLOSE;
    this->parserRecord->contentLen = 0;

    // 初始化其他属性
    this->efd = efd;
    this->cfd = cfd;
    this->readIndx = 0;
    this->curReadIndx = 0;
    this->curLineBegin = 0;
    this->writeIndx = 0;
    this->curWriteIndx = 0;
    memset(this->readBuffer, '\0', maxBuffSize);
    memset(this->writeBuffer, '\0', maxBuffSize);

    // client数量+1
    connNum++;
}

void httpConn::resetConn() {
    // 长连接的情况下重置连接的属性
    this->parserRecord->parserStatus = CHECK_REQUEST_LINE;
    memset(this->parserRecord->RequestPath, '\0', maxPathLen);
    memset(this->parserRecord->httpProt, '\0', 10);
    this->parserRecord->contentLen = 0;

    this->readIndx = 0;
    this->curReadIndx = 0;
    this->curLineBegin = 0;
    this->writeIndx = 0;
    this->curWriteIndx = 0;
    memset(this->readBuffer, '\0', maxBuffSize);
    memset(this->writeBuffer, '\0', maxBuffSize);
}

void httpConn::resetfd(EPOLL_EVENTS eventStatus) {
    epoll_event event;
    event.data.fd = this->cfd;
    event.events = eventStatus | EPOLLET | EPOLLONESHOT;
    epoll_ctl(this->efd, EPOLL_CTL_MOD, this->cfd, &event);
}

void httpConn::closeConn() {
    epoll_ctl(this->efd, EPOLL_CTL_DEL, this->cfd, 0);
    close(this->cfd);
    // client数量-1
    connNum--;
    // printf("server close..\n");
}

void httpConn::readData() {
    while (true) {
        int ret = recv(this->cfd, this->readBuffer+readIndx, maxBuffSize-readIndx, 0);
        if (ret < 0) {
            // 数据读完后，直接break
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                break;
            }
            // 如果被信号中断，重置继续读
            if (errno == EINTR) {
                continue;
            }
            close(this->cfd);
            perror("read data error.");
            break;
        }
        else if (ret == 0) {
            close(this->cfd);
            // printf("client close.\n");
            break;
        }
        else {
            this->readIndx += ret;
        }
    }
}

inline
char* httpConn::getLine() {
    return this->readBuffer + this->curLineBegin;
}

httpConn::SUBSTATUS httpConn::readLine() {
    char tmpChr;
    for (; this->curReadIndx < this->readIndx; this->curReadIndx++) {
        tmpChr = this->readBuffer[this->curReadIndx];
        if (tmpChr == '\r') {
            if ((this->curReadIndx+1) == this->readIndx)
                return LINE_OPEN;
            else if (this->readBuffer[this->curReadIndx+1] == '\n') {
                this->readBuffer[this->curReadIndx++] = '\0';
                this->readBuffer[this->curReadIndx++] = '\0';
                return LINE_OK;
            }
            else
                return LINE_BAD;
        }
        else if (tmpChr == '\n') {
            if (this->curReadIndx > 1 && this->readBuffer[this->curReadIndx-1] == '\r') {
                this->readBuffer[this->curReadIndx-1] = '\0';
                this->readBuffer[this->curReadIndx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

httpConn::HTTPCODE httpConn::parseLine(char* text) {
    // 匹配空格或\t
    char* path = strpbrk(text, " \t");
    // 没有就是格式错误
    if (!path)
        return BAD_REQUEST;
    // 在空格位填\0，取出前面的字符串就是method
    *path++ = '\0';
    if (strcasecmp(text, "GET") == 0)
        this->parserRecord->method = GET;
    else if (strcasecmp(text, "POST") == 0)
        this->parserRecord->method = POST;
    else
        return BAD_REQUEST;

    // 取出method后，后面还可能有空格或\t，跳过直到第一个非空字符 
    path += strspn(path, " \t");
    // 从非空字符开始匹配到第一个空格或\t
    char* prot = strpbrk(path, " \t");
    if (!prot)
        return BAD_REQUEST;
    // 在空字符处填\0，取出前面的字符串，即path
    *prot++ = '\0';

    // 对于path的额外处理
    if (strncasecmp(path, "http://", 7) == 0) {
        path += 7;
        path = strchr(path, '/');
    }
    if (strncasecmp(path, "https://", 8) == 0) {
        path += 8;
        path = strchr(path, '/');
    }
    if (!path || path[0]!='/')
        return BAD_REQUEST;
    strcpy(this->parserRecord->RequestPath, path);

    // 取出path后，后面还可能有空格或\t，跳过直到第一个非空字符
    prot += strspn(prot, " \t");
    // 匹配http协议版本
    if (strcasecmp(prot, "HTTP/1.1") == 0)
        strcpy(this->parserRecord->httpProt, prot);
    else
        return BAD_REQUEST;

    this->parserRecord->parserStatus = CHECK_REQUEST_HEADER;
    return GET_REQUEST;
}

httpConn::HTTPCODE httpConn::parseHeader(char* text) {
    if (text[0] == '\0') {
        if (this->parserRecord->contentLen != 0) {
            this->parserRecord->parserStatus = CHECK_REQUEST_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;

        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
            this->parserRecord->conn = KEEPALIVE;
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        this->parserRecord->contentLen = atol(text);
    }
    return NO_REQUEST;
}

httpConn::HTTPCODE httpConn::parseContent(char* text) {
    if (this->readIndx >= (this->parserRecord->contentLen+this->curReadIndx)) {
        text[this->parserRecord->contentLen] = '\0';
        this->parserRecord->content = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

httpConn::HTTPCODE httpConn::httpParser() {
    SUBSTATUS lineStatus;
    HTTPCODE status = NO_REQUEST;
    char* text = nullptr;

    while ((this->parserRecord->parserStatus == CHECK_REQUEST_CONTENT)
            ||
           ((lineStatus=readLine()) == LINE_OK)) {

        text = getLine();
        this->curLineBegin = this->curReadIndx;

        switch (this->parserRecord->parserStatus) {
        case CHECK_REQUEST_LINE: {
            status = parseLine(text);
            if (status == BAD_REQUEST)
                return status;
            break;
        }
        case CHECK_REQUEST_HEADER: {
            status = parseHeader(text);
            if (status == GET_REQUEST)
                return status;
            break;
        }
        case CHECK_REQUEST_CONTENT: {
            status = parseContent(text);
            return status;
            break;
        }
        default:
            break;
        }
    }
    return NO_REQUEST;
}

bool httpConn::writeData(HTTPCODE status) {
    sprintf(this->writeBuffer, 
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: %ld\r\n\r\n",
            strlen(this->readBuffer));
    strcat(this->writeBuffer, this->readBuffer);
    send(this->cfd, this->writeBuffer, strlen(this->writeBuffer), 0);
    return true;
}

void httpConn::processConn() {
    while (this->curReadIndx != this->readIndx) {
        HTTPCODE parseStatus = httpParser();
        if (parseStatus == NO_REQUEST) {
            resetfd(EPOLLIN);
            return;
        }
        bool writeStatus = writeData(parseStatus);
        if (!writeStatus) {
            resetfd(EPOLLOUT);
            return;
        }
    }
    if (this->parserRecord->conn == KEEPALIVE) {
        resetConn();
        resetfd(EPOLLIN);
        return;
    }
    else {
        closeConn();
        return;
    } 
}

void httpConn::processRead() {
    readData();
    processConn();
}

void httpConn::processWrite() {
    bool writeStatus = writeData(GET_REQUEST);
    if (!writeStatus) {
        resetfd(EPOLLOUT);
        return;
    } 
    processConn();
}
