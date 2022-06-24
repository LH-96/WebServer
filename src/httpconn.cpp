#include "httpconn.h"

/**
 * @brief 在建立一个http连接时，初始化一个http对象(全部参数初始化，防止fd被重用后，上次残留数据)
 * 
 * @param efd 
 * @param cfd 
 */
void httpConn::init(int efd, int cfd) {
    this->efd = efd;
    this->cfd = cfd;
    this->curReadIndx = 0;
    this->curWriteIndx = 0;
    memset(this->readBuffer, '\0', maxBuffSize);
    memset(this->writeBuffer, '\0', maxBuffSize);
}

void httpConn::resetfd() {
    epoll_event event;
    event.data.fd = this->cfd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(this->efd, EPOLL_CTL_MOD, this->cfd, &event);
}

void httpConn::readData() {
    while (true) {
        int ret = recv(this->cfd, this->readBuffer+curReadIndx, maxBuffSize-curReadIndx, 0);
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
            break;
        }
        else if (ret == 0) {
            close(this->cfd);
            printf("client close.\n");
            break;
        }
        else {
            this->curReadIndx += ret;
        }
    }
}

void httpConn::parseLine() {

}

bool httpConn::httpParser() {
    while (true) {
        auto mainState = CHECK_REQUEST_LINE;
        switch (mainState) {
        case CHECK_REQUEST_LINE:
        case CHECK_REQUEST_HEADER:
        default:
            break;
        }
    }
}

bool httpConn::writeData() {
    strcpy(this->writeBuffer, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n");
    strcat(this->writeBuffer, this->readBuffer);
    send(this->cfd, this->writeBuffer, strlen(this->writeBuffer), 0);
    return true;
}

void httpConn::processConn() {
    readData();
    resetfd();
    writeData();
}
