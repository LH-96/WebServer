#include "httpconn.h"

const char *httpConn::rootPath = "/home/leo/WebServer/root";
std::atomic_int httpConn::connNum(0);

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";

/**
 * @brief 在建立一个http连接时，初始化一个http对象(全部参数初始化)
 * 
 * @param efd 
 * @param cfd 
 */
void httpConn::init(int efd, int cfd) {
    // 初始化http解析信息，默认get方法，短连接
    this->parserRecord->parserStatus = CHECK_REQUEST_LINE;
    this->parserRecord->method = GET;
    memset(this->parserRecord->requestPath, '\0', maxPathLen);
    memset(this->parserRecord->requestUrl, '\0', maxUrlLen);
    this->parserRecord->fileMMAP = nullptr;
    memset(&this->parserRecord->fileStat, 0, sizeof(struct stat));
    memset(this->parserRecord->httpProt, '\0', 10);
    this->parserRecord->conn = CLOSE;
    this->parserRecord->contentLen = 0;
    this->parserRecord->content = nullptr;

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

    // client数量+1
    connNum++;
}

void httpConn::resetConn() {
    // 长连接的情况下重置连接的属性
    this->parserRecord->parserStatus = CHECK_REQUEST_LINE;
    this->parserRecord->method = GET;
    memset(this->parserRecord->requestPath, '\0', maxPathLen);
    memset(this->parserRecord->requestUrl, '\0', maxUrlLen);
    if (this->parserRecord->fileMMAP) {
        munmap(this->parserRecord->fileMMAP, this->parserRecord->fileStat.st_size);
        this->parserRecord->fileMMAP = nullptr;
    }
    memset(&this->parserRecord->fileStat, 0, sizeof(struct stat));
    memset(this->parserRecord->httpProt, '\0', 10);
    this->parserRecord->contentLen = 0;
    this->parserRecord->content = nullptr;

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
        // printf("server close..\n");
    }
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
            // closeConn();
            perror("read data error.");
            break;
        }
        else if (ret == 0) {
            // closeConn();
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
    strcpy(this->parserRecord->requestUrl, path);

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

httpConn::HTTPCODE httpConn::doRequest() {
    strcpy(this->parserRecord->requestPath, rootPath);
    int len = strlen(rootPath);

    // 查找请求url中 / 的位置
    const char *p = strrchr(this->parserRecord->requestUrl, '/');

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
    strncpy(this->parserRecord->requestPath+len, url, strlen(url));

    // 用stat结构体存储文件信息
    stat(this->parserRecord->requestPath, &this->parserRecord->fileStat);

    // 判断文件是否可读，不可读返回FORBIDDEN_REQUEST
    if (!(this->parserRecord->fileStat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    // 用只读方式获取文件fd，用mmap映射到内存中
    int ffd = open(this->parserRecord->requestPath, O_RDONLY);
    this->parserRecord->fileMMAP = (char*)mmap(0, this->parserRecord->fileStat.st_size, 
                                    PROT_READ, MAP_PRIVATE, 
                                    ffd, 0);
    close(ffd);
    return strcmp(url, "/404.html")==0 ? NO_RESOURCE : FILE_REQUEST;
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
            break;
        }
        default:
            break;
        }
    }
    return NO_REQUEST;
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
    return addResponse("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool httpConn::addContentLen(int contentLen) {
    return addResponse("Content-Length:%d\r\n", contentLen);
}

bool httpConn::addContentType() {
    return addResponse("Content-Type:%s\r\n","text/html");
}

bool httpConn::addConn() {
    return addResponse("Connection:%s\r\n",
                        (this->parserRecord->conn==KEEPALIVE)?"keep-alive":"close");
}

bool httpConn::addBlankLine() {
    return addResponse("%s","\r\n");
}

void httpConn::addHeader(int contentLen) {
    // addContentType();
    addConn();
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
        addHeader(this->parserRecord->fileStat.st_size);
        // iovec[0]指向响应报文缓冲区，长度是writeindex
        this->iv[0].iov_base = this->writeBuffer;
        this->iv[0].iov_len = this->writeIndx;
        // iovec[1]指向mmap返回的文件指针，长度是文件大小
        this->iv[1].iov_base = this->parserRecord->fileMMAP;
        this->iv[1].iov_len = this->parserRecord->fileStat.st_size;
        this->ivCount = 2;
        this->sendBytes = this->writeIndx + this->parserRecord->fileStat.st_size;
        return true;
    }
    // 可访问文件，200
    case FILE_REQUEST: {
        addLine(200, ok_200_title);
        addHeader(this->parserRecord->fileStat.st_size);
        // iovec[0]指向响应报文缓冲区，长度是writeindex
        this->iv[0].iov_base = this->writeBuffer;
        this->iv[0].iov_len = this->writeIndx;
        // iovec[1]指向mmap返回的文件指针，长度是文件大小
        this->iv[1].iov_base = this->parserRecord->fileMMAP;
        this->iv[1].iov_len = this->parserRecord->fileStat.st_size;
        this->ivCount = 2;
        this->sendBytes = this->writeIndx + this->parserRecord->fileStat.st_size;
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
    int temp = 0, newadd = 0;

    // 如果发送的数据长度为0，直接返回
    if (this->sendBytes == 0)
        return true;
    
    while (true) {
        // writev函数集中写，发送数据到client
        temp = writev(this->cfd, this->iv, this->ivCount);

        // 正常发送，temp是发送字节数
        if (temp > 0) {
            // 更新已发送字节
            this->curWriteIndx += temp;
            // 偏移文件iovec指针
            newadd = this->curWriteIndx - this->writeIndx;
        }
        if (temp <= -1) {
            // 判断缓冲区是否已满
            if (errno == EAGAIN) {
                // printf("send EAGAIN...\n");
                // iovec[0]的内容已发完的情况
                if (this->curWriteIndx >= this->iv[0].iov_len) {
                    // 不再发送头部信息
                    this->iv[0].iov_len = 0;
                    this->iv[1].iov_base = this->parserRecord->fileMMAP + newadd;
                    this->iv[1].iov_len = this->sendBytes;
                }
                // iovec[0]的内容没发完
                else {
                    this->iv[0].iov_base = this->writeBuffer + this->sendBytes;
                    this->iv[0].iov_len = this->iv[0].iov_len - this->curWriteIndx;
                }
                return false;
            }
            // 发送失败，但不是缓冲区问题，取消mmap
            munmap(this->parserRecord->fileMMAP, this->parserRecord->fileStat.st_size);
            this->parserRecord->fileMMAP = nullptr;
            return true;
        }

        // 更新已发送字节
        this->sendBytes -= temp;

        // 数据全部发送完成
        if (this->sendBytes <= 0) {
            munmap(this->parserRecord->fileMMAP, this->parserRecord->fileStat.st_size);
            this->parserRecord->fileMMAP = nullptr;
            return true; 
        }
    }
    return true;
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
    // printf("resend...\n");
    bool writeStatus = writeData();
    if (!writeStatus) {
        resetfd(EPOLLOUT);
        return;
    } 
    processConn();
}
