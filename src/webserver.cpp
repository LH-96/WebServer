#include "webserver.h"

/**
 * @brief 创建监听fd
 * 
 * @return 监听fd
 */
int webserver::initListenfd() {
    int ret; //检查函数返回值

    // string->const char*
    const char* ip = this->ip.c_str();
    const char* port = this->port.c_str();

    // 创建监听socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (listenfd == -1) {
        perror("listenfd created failed.");
        return -1;
    }

    // 端口复用，防止server主动关闭后，不能立即重启
    int opt = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
    if (ret == -1) {
        perror("set addr reuse fail.");
        return -1;
    }

    // 绑定地址结构
    struct sockaddr_in laddr;
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(atoi(port));
    ret = inet_pton(AF_INET, ip, (void*)&laddr.sin_addr);
    if (ret != 1) {
        perror("ip convert fail.");
        return -1;
    }
    ret = bind(listenfd, (struct sockaddr*)&laddr, sizeof(laddr));
    if (ret == -1) {
        perror("bind fail.");
        return -1;
    }

    // 设置监听fd的数量
    ret = listen(listenfd, 128);
    if (ret == -1) {
        perror("listen fail.");
        return -1;
    }

    return listenfd;
}

/**
 * @brief 设置fd为非阻塞
 * 
 * @param fd 需要设置的fd
 */
void webserver::setNonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

/**
 * @brief 将fd添加进epoll树中，fd自动设置为非阻塞，可选择是否为ET模式
 * 
 * @param efd epoll树根节点
 * @param fd 需要添加的fd
 */
void webserver::addfd(const int &efd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = this->trigger == "ET" ? EPOLLIN | EPOLLET : EPOLLIN;  // 是否设置ET
    int ret = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);    // 添加进树中
    if (ret == -1) {
        perror("epoll add fail.");
        return;
    }
    setNonblocking(fd);
}

/**
 * @brief 读数据，ET模式或LT模式
 * 
 * @param buf 用户缓冲区
 * @param cfd client fd
 */
void webserver::readData(char *buf, int cfd) {
    printf("event trigger once.\n");
    if (this->trigger == "ET") {
        while (true) {
            memset(buf, '\0', sizeof(buf)); // 每次读前都将用户缓冲区重置
            int ret = recv(cfd, buf, MAX_BUFF_SIZE-1, 0);
            if (ret < 0) {
                // 数据读完后，直接break，让epoll继续监听
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    // perror("read done.");
                    break;
                }
                // 如果被信号中断，重置继续读
                if (errno == EINTR) {
                    // perror("read interrupted.");
                    continue;
                }
                close(cfd);
                perror("recv < 0.");
                break;
            }
            else if (ret == 0) {
                close(cfd);
                printf("client close.\n");
                break;
            }
            else {
                printf("get %d bytes of content: %s\n", ret, buf);
            }
        }
    }
    else {
        memset(buf, '\0', sizeof(buf));
        int ret = recv(cfd, buf, MAX_BUFF_SIZE-1, 0);
        if (ret <= 0) {
            close(cfd);
            printf("client close.\n");
            return;
        }
        printf("get %d bytes of content: %s\n", ret, buf);
    }
}

/**
 * @brief 在epoll监听到listenfd的事件后，与client建立连接，并将对应的cfd添加到epoll树中
 * 
 * @param efd epoll根节点
 * @param listenfd listenfd
 */
void webserver::buildConn(const int &efd, int listenfd) {
    // client 地址结构 和 fd
    struct sockaddr_in caddr;
    bzero((void*)&caddr, sizeof(caddr));
    socklen_t caddrLen;
    int cfd;

    // 设置accept循环，接收client连接（因为listenfd可能是ET模式）
    while (true) {
        caddrLen = sizeof(caddrLen);
        cfd = accept(listenfd, (struct sockaddr*)&caddr, &caddrLen);
        if (cfd == -1) {
            if ((errno==EAGAIN) || (errno==EWOULDBLOCK)) {
                perror("accept done, no more client.");
                break;
            }
            if (errno == EINTR) {
                perror("accept was interrupted, continue...");
                continue;
            }
        }
        addfd(efd, cfd);
        printf("New client connect...\n");
    }
}

/**
 * @brief epoll循环处理事件， 若fd是listenfd就创建新连接并添加进树中；
 * 若不是listenfd，是读事件，就执行对应任务。
 * 
 * @param events 有事件发生的fd数组
 * @param eventsLen fd数组的长度
 * @param efd epoll根节点
 * @param listenfd listenfd
 */
void webserver::epollHandler(const epoll_event *events, const int &eventsLen, 
                             const int &efd, const int &listenfd) {
    // buffer
    char buf[MAX_BUFF_SIZE];

    // epoll loop
    for (int i = 0; i < eventsLen; i++) {
        int socketfd = events[i].data.fd;
        if (socketfd == listenfd) {
            buildConn(efd, socketfd);
        }
        else if (events[i].events & EPOLLIN) {
            readData(buf, socketfd);
        }
        else {
            printf("epollHandler error.\n");
        }
    }
}

/**
 * @brief 主线程，创建listenfd，epoll树，并开始epoll_wait循环监听
 * 
 */
void webserver::run() {
    // listenfd
    int listenfd = initListenfd();

    // epoll
    epoll_event events[MAX_EVENT_NUMBER];
    int efd = epoll_create(8);
    if (efd == -1) {
        perror("epoll_create fail.");
        return;
    }
    addfd(efd, listenfd);

    while (true) {
        int ret = epoll_wait(efd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0) {
            perror("epoll_wait fail.");
            return;
        }
        epollHandler(events, ret, efd, listenfd);
    }
}