#include "httpconn.h"

const char *httpConn::rootPath = "/home/leo/WebServer/root";
std::atomic_int httpConn::connNum(0);

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *range_206_title = "Partial Content";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";

/**
 * @brief 在建立一个http连接时，初始化一个http对象(全部参数初始化)
 * 
 * @param efd 
 * @param cfd 
 */
void httpConn::init(int efd, int cfd, int isCloseLog) {
    // 初始化http解析信息，默认get方法，短连接
    parserRecord->parserStatus = CHECK_REQUEST_LINE;
    parserRecord->method = GET;
    memset(parserRecord->requestPath, '\0', maxPathLen);
    memset(parserRecord->requestUrl, '\0', maxUrlLen);
    unMMap();
    memset(parserRecord->fileType, '\0', sizeof(parserRecord->fileType));
    parserRecord->isRangeTransp = false;
    parserRecord->rangeBegin = 0;
    parserRecord->rangeEnd = 0;
    memset(&parserRecord->fileStat, 0, sizeof(struct stat));
    memset(parserRecord->httpProt, '\0', sizeof(parserRecord->httpProt));
    parserRecord->conn = CLOSE;
    parserRecord->contentLen = 0;
    parserRecord->content = nullptr;

    // 初始化其他属性
    this->efd = efd;
    this->cfd = cfd;
    this->readIndx = 0;
    this->curReadIndx = 0;
    this->curLineBegin = 0;
    this->writeIndx = 0;
    this->curWriteIndx = 0;
    this->ivCount = 0;
    this->sendBytes = 0;
    this->isclose = false;
    memset(this->iv, 0, sizeof(struct iovec)*2);
    memset(this->readBuffer, '\0', maxBuffSize);
    memset(this->writeBuffer, '\0', maxBuffSize);

    this->closeLog = isCloseLog;

    // client数量+1
    connNum++;
}

void httpConn::resetConn(bool isResetWriteOnly) {
    parserRecord->parserStatus = CHECK_REQUEST_LINE;
    memset(parserRecord->requestPath, '\0', maxPathLen);
    memset(parserRecord->requestUrl, '\0', maxUrlLen);
    unMMap();
    memset(&parserRecord->fileStat, 0, sizeof(struct stat));
    memset(parserRecord->fileType, '\0', sizeof(parserRecord->fileType));
    memset(parserRecord->httpProt, '\0', sizeof(parserRecord->httpProt));
    parserRecord->isRangeTransp = false;
    parserRecord->rangeBegin = 0;
    parserRecord->rangeEnd = 0;
    parserRecord->contentLen = 0;
    parserRecord->content = nullptr;

    if (!isResetWriteOnly) {
        this->readIndx = 0;
        this->curReadIndx = 0;
        this->curLineBegin = 0;
        memset(this->readBuffer, '\0', maxBuffSize);
    }
    this->writeIndx = 0;
    this->curWriteIndx = 0;
    this->ivCount = 0;
    this->sendBytes = 0;
    memset(this->iv, 0, sizeof(struct iovec)*2);
    memset(this->writeBuffer, '\0', maxBuffSize);
}

void httpConn::resetfd(EPOLL_EVENTS eventStatus) {
    epoll_event event;
    event.data.fd = this->cfd;
    event.events = eventStatus | EPOLLET | EPOLLONESHOT;
    epoll_ctl(this->efd, EPOLL_CTL_MOD, this->cfd, &event);
}

void httpConn::closeConn() {
    if (this->isclose == false) {
        this->isclose = true;
        epoll_ctl(this->efd, EPOLL_CTL_DEL, this->cfd, 0);
        close(this->cfd);
        // client数量-1
        connNum--;
        LOG_INFO("close fd:%d, clients:%d", this->cfd, int(connNum));
        // printf("server close..\n");
    }
}

bool httpConn::readData() {
    while (true) {
        int ret = recv(this->cfd, this->readBuffer+readIndx, maxBuffSize-readIndx, 0);
        if (ret < 0) {
            // 数据读完后，直接break
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                return true;
            }
            // 如果被信号中断，重置继续读
            if (errno == EINTR) {
                continue;
            }
            closeConn();
            // perror("read data error.");
            return false;
        }
        else if (ret == 0) {
            closeConn();
            // printf("client close.\n");
            return false;
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
        parserRecord->method = GET;
    else if (strcasecmp(text, "POST") == 0)
        parserRecord->method = POST;
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
    strcpy(parserRecord->requestUrl, path);

    // 取出path后，后面还可能有空格或\t，跳过直到第一个非空字符
    prot += strspn(prot, " \t");
    // 匹配http协议版本
    if (strcasecmp(prot, "HTTP/1.1") == 0)
        strcpy(parserRecord->httpProt, prot);
    else
        return BAD_REQUEST;

    parserRecord->parserStatus = CHECK_REQUEST_HEADER;
    return GET_REQUEST;
}

httpConn::HTTPCODE httpConn::parseHeader(char* text) {
    if (text[0] == '\0') {
        if (parserRecord->contentLen != 0) {
            parserRecord->parserStatus = CHECK_REQUEST_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;

        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
            parserRecord->conn = KEEPALIVE;
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        parserRecord->contentLen = atol(text);
    }
    else if (strncasecmp(text, "Range:", 6) == 0) {
        // Range:bytes=0-123
        text += 13;
        char* tmp = nullptr;  // 使用strtoul后，tmp指向第一个非数字位置（—）
        parserRecord->isRangeTransp = true;
        parserRecord->rangeBegin = strtoul(text, &tmp, 0);
        if (strlen(tmp) == 1) {
            // Range:bytes=0- ，将rangeEnd暂设为-1，后面获取请求文件再设为文件大小-1
            parserRecord->rangeEnd = -1;
        }
        else {
            text = tmp + 1;
            tmp = nullptr;
            parserRecord->rangeEnd = strtoul(text, &tmp, 0);
        }
    }
    return NO_REQUEST;
}

httpConn::HTTPCODE httpConn::parseContent(char* text) {
    if (this->readIndx >= (parserRecord->contentLen+this->curReadIndx)) {
        parserRecord->content = text;
        this->curReadIndx += parserRecord->contentLen;
        this->curLineBegin += parserRecord->contentLen;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

httpConn::HTTPCODE httpConn::doRequest() {
    strcpy(parserRecord->requestPath, rootPath);
    int len = strlen(rootPath);

    // 查找请求url中 / 的位置
    const char *p = strrchr(parserRecord->requestUrl, '/');

    // 登录，注册校验

    char url[100];
    // 如果是 /，表示默认页面
    if (strlen(p) == 1 && *p == '/') {
        strcpy(url, "/welcome.html");
    }
    // 如果是 /0，表示注册页面
    else if (strlen(p) == 2 && *(p+1) == '0') {
        strcpy(url, "/register.html");
    }
    // 如果是 /1，表示登录页面
    else if (strlen(p) == 2 && *(p+1) == '1') {
        strcpy(url, "/log.html");
    }
    else if (strlen(p) == 2 && *(p+1) == 'v') {
        strcpy(url, "/video.html");
    }
    else if (!strcmp(p, "/xxx.mp4")) {
        strcpy(url, "/xxx.mp4");
    }
    // 无资源
    else {
        strcpy(url, "/404.html");
    }
    // 拼接最终访问文件path
    strncpy(parserRecord->requestPath+len, url, strlen(url));

    // 用stat结构体存储文件信息
    stat(parserRecord->requestPath, &parserRecord->fileStat);

    // 判断文件是否可读，不可读返回FORBIDDEN_REQUEST
    if (!(parserRecord->fileStat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    // 获取文件类型
    p = strrchr(parserRecord->requestPath, '.');
    strcpy(parserRecord->fileType, p+1);

    // 用只读方式获取文件fd，用mmap映射到内存中
    int ffd = open(parserRecord->requestPath, O_RDONLY);
    parserRecord->fileMMAP = (char*)mmap(0, parserRecord->fileStat.st_size, 
                                    PROT_READ, MAP_PRIVATE, 
                                    ffd, 0);
    close(ffd);

    // 如果是range方式传输，且range尾端未知（-1），设为文件大小-1(因为是从0开始索引)
    if (parserRecord->isRangeTransp && (parserRecord->rangeEnd == -1)) {
        parserRecord->rangeEnd = parserRecord->fileStat.st_size -1;
    }

    return strcmp(url, "/404.html")==0 ? NO_RESOURCE : FILE_REQUEST;
}

httpConn::HTTPCODE httpConn::httpParser() {
    SUBSTATUS lineStatus;
    HTTPCODE status = NO_REQUEST;
    char* text = nullptr;

    while ((parserRecord->parserStatus == CHECK_REQUEST_CONTENT)
            ||
           ((lineStatus=readLine()) == LINE_OK)) {

        text = getLine();
        this->curLineBegin = this->curReadIndx;

        switch (parserRecord->parserStatus) {
        case CHECK_REQUEST_LINE: {
            status = parseLine(text);
            if (status == BAD_REQUEST)
                return status;
            break;
        }
        case CHECK_REQUEST_HEADER: {
            status = parseHeader(text);
            if (status == BAD_REQUEST)
                return status;
            else if (status == GET_REQUEST)
                return doRequest();
            break;
        }
        case CHECK_REQUEST_CONTENT: {
            status = parseContent(text);
            if (status == GET_REQUEST)
                return doRequest();
            else
                return NO_REQUEST;
            break;
        }
        default:
            break;
        }
    }
    return NO_REQUEST;
}

bool httpConn::unMMap() {
    if (parserRecord->fileMMAP) {
        munmap(parserRecord->fileMMAP, parserRecord->fileStat.st_size);
        parserRecord->fileMMAP = nullptr;
        return true;
    }
    return false;
}

bool httpConn::addResponse(const char* format, ...) {
    if (this->writeIndx >= maxBuffSize)
        return false;

    va_list argList;
    va_start(argList, format);

    // 将数据format写入缓冲区，返回写法长度
    int len = vsnprintf(this->writeBuffer+this->writeIndx, maxBuffSize-1-this->writeIndx,
                        format, argList);
    if (len >= (maxBuffSize-1-this->writeIndx)) {
        va_end(argList);
        return false;
    }

    // 更新writeindex的位置
    this->writeIndx += len;
    // 清空变参列表
    va_end(argList);

    return true;
}

bool httpConn::addLine(int status, const char* title) {
    return addResponse("%s %d %s\r\n","HTTP/1.1", status, title);
}

bool httpConn::addContentLen(int contentLen) {
    return addResponse("Content-Length:%d\r\n", contentLen);
}

bool httpConn::addContentRange() {
    return addResponse("Content-Range:bytes %lu-%lu/%lu\r\n",
                       parserRecord->rangeBegin,
                       parserRecord->rangeEnd,
                       parserRecord->fileStat.st_size);
}

bool httpConn::addContentType() {
    if (!strcmp(parserRecord->fileType, "html"))
        return addResponse("Content-Type:%s\r\n", "text/html");
    else if (!strcmp(parserRecord->fileType, "mp4"))
        return addResponse("Content-Type:%s\r\n", "video/mp4");
    return true;
}

bool httpConn::addConn() {
    return addResponse("Connection:%s\r\n",
                        (parserRecord->conn==KEEPALIVE)?"keep-alive":"close");
}

bool httpConn::addBlankLine() {
    return addResponse("%s", "\r\n");
}

void httpConn::addHeader(int contentLen) {
    addContentType();
    addConn();
    if (parserRecord->isRangeTransp) {
        addContentRange();
    }
    addContentLen(contentLen);
    addBlankLine();
}

bool httpConn::addContent(const char* content) {
    return addResponse("%s",content);
}

bool httpConn::mergeResponse(HTTPCODE ret) {
    switch(ret) {
    // 报文语法错误，400
    case BAD_REQUEST: {
        addLine(400, error_400_title);
        addHeader(strlen(error_400_form));
        if (!addContent(error_400_form))
            return false;
        break;
    }
    // 没有权限访问资源，403
    case FORBIDDEN_REQUEST: {
        addLine(403, error_403_title);
        addHeader(strlen(error_403_form));
        if (!addContent(error_403_form))
            return false;
        break;
    }
    // 请求错误文件，404
    case NO_RESOURCE: {
        addLine(404, error_404_title);
        addHeader(parserRecord->fileStat.st_size);
        // iovec[0]指向响应报文缓冲区，长度是writeindex
        this->iv[0].iov_base = this->writeBuffer;
        this->iv[0].iov_len = this->writeIndx;
        // iovec[1]指向mmap返回的文件指针，长度是文件大小
        this->iv[1].iov_base = parserRecord->fileMMAP;
        this->iv[1].iov_len = parserRecord->fileStat.st_size;
        this->ivCount = 2;
        this->sendBytes = this->writeIndx + parserRecord->fileStat.st_size;
        return true;
    }
    // 可访问文件，200或206
    case FILE_REQUEST: {
        if (parserRecord->isRangeTransp) {
            addLine(206, range_206_title);
            addHeader(parserRecord->rangeEnd - parserRecord->rangeBegin + 1);
            // iovec[0]指向响应报文缓冲区，长度是writeindex
            this->iv[0].iov_base = this->writeBuffer;
            this->iv[0].iov_len = this->writeIndx;
            // iovec[1]指向mmap返回的文件指针，长度是文件大小与range偏移量
            this->iv[1].iov_base = parserRecord->fileMMAP + parserRecord->rangeBegin;
            this->iv[1].iov_len = parserRecord->rangeEnd - parserRecord->rangeBegin + 1;
            this->ivCount = 2;
            this->sendBytes = this->writeIndx + parserRecord->rangeEnd - parserRecord->rangeBegin + 1;
        }
        else {
            addLine(200, ok_200_title);
            addHeader(parserRecord->fileStat.st_size);
            // iovec[0]指向响应报文缓冲区，长度是writeindex
            this->iv[0].iov_base = this->writeBuffer;
            this->iv[0].iov_len = this->writeIndx;
            // iovec[1]指向mmap返回的文件指针，长度是文件大小
            this->iv[1].iov_base = parserRecord->fileMMAP;
            this->iv[1].iov_len = parserRecord->fileStat.st_size;
            this->ivCount = 2;
            this->sendBytes = this->writeIndx + parserRecord->fileStat.st_size;
        }
        return true;
    }
    default:
        return false;
    }

    // 除了请求文件，其他状态只申请一个iovec
    this->iv[0].iov_base = this->writeBuffer;
    this->iv[0].iov_len = this->writeIndx;
    this->ivCount = 1;
    return true;
}

bool httpConn::writeData() {
    int temp = 0;

    // 如果发送的数据长度为0，直接返回
    if (this->sendBytes == 0)
        return true;
    
    while (true) {
        // writev函数集中写，发送数据到client
        temp = writev(this->cfd, this->iv, this->ivCount);

        if (temp < 0) {
            if (errno == EAGAIN) {
                return false;
            }
            unMMap();
            return true;
        }

        // 更新已发送字节
        this->curWriteIndx += temp;
        this->sendBytes -= temp;
        if (this->curWriteIndx >= this->iv[0].iov_len) {
            // 不再发送头部信息
            this->iv[0].iov_len = 0;
            if (parserRecord->isRangeTransp) {
                this->iv[1].iov_base = parserRecord->fileMMAP 
                                        + (this->curWriteIndx-this->writeIndx)
                                        + parserRecord->rangeBegin;
            }
            else {
                this->iv[1].iov_base = parserRecord->fileMMAP + (this->curWriteIndx-this->writeIndx);
            }
            this->iv[1].iov_len = this->sendBytes;
        }
        // iovec[0]的内容没发完
        else {
            this->iv[0].iov_base = this->writeBuffer + this->curWriteIndx;
            this->iv[0].iov_len = this->iv[0].iov_len - this->curWriteIndx;
        }

        // 数据全部发送完成
        if (this->sendBytes <= 0) {
            unMMap();
            return true; 
        }
    }
}

void httpConn::processConn() {
    while (this->curReadIndx != this->readIndx) {
        HTTPCODE parseStatus = httpParser();
        if (parseStatus == NO_REQUEST) {
            resetfd(EPOLLIN);
            return;
        }
        mergeResponse(parseStatus);
        bool writeStatus = writeData();
        if (!writeStatus) {
            resetfd(EPOLLOUT);
            return;
        }
        resetConn(true);
    }
    if (parserRecord->conn == KEEPALIVE) {
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
    if (!readData()) {
        return;
    }
    processConn();
}

void httpConn::processWrite() {
    // printf("resend...\n");
    bool writeStatus = writeData();
    if (!writeStatus) {
        resetfd(EPOLLOUT);
        return;
    }
    resetConn(true);
    processConn();
}
