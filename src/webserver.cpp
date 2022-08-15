#include "webserver.h"

extern const int maxEventNumber = 10240;
extern const int maxHttpConn = 10240;

/**
 * @brief 创建监听fd
 * 
 * @return 监听fd
 */
int webserver::initListenfd() {
    int ret; //检查函数返回值

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
void webserver::addfd(int efd, int fd, bool isOneshot = false) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;  // 设置ET
    if (isOneshot)
        event.events |= EPOLLONESHOT;
    int ret = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);    // 添加进树中
    if (ret == -1) {
        perror("epoll add fail.");
        return;
    }
    setNonblocking(fd);
}

/**
 * @brief 在epoll监听到listenfd的事件后，与client建立连接，并将对应的cfd添加到epoll树中
 * 
 * @param efd epoll根节点
 * @param listenfd listenfd
 */
void webserver::buildConn(int efd, int listenfd) {
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
                // perror("accept done, no more client.");
                break;
            }
            if (errno == EINTR) {
                // perror("accept was interrupted, continue...");
                continue;
            }
            if (errno == EMFILE) {
                // 当进程打开的fd达到上限时，用这个fd接受连接再马上关闭
                close(this->backupfd);  
                this->backupfd = accept(listenfd, NULL, NULL); 
                close(this->backupfd);  
                this->backupfd = open("/dev/null", O_RDONLY | O_CLOEXEC);  
                continue;
            }
        }
        if (httpConn::connNum >= maxHttpConn) {
            close(cfd);
            printf("too much clients...\n");
            continue;
        }
        this->clients[cfd].init(efd, cfd);
        addfd(efd, cfd, true);
        // printf("New client connect...\n");
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
                             int efd, int listenfd) {
    // epoll loop
    for (int i = 0; i < eventsLen; i++) {
        int socketfd = events[i].data.fd;
        if (socketfd == listenfd) {
            buildConn(efd, socketfd);
        }
        else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
            close(socketfd);
        }
        else if (events[i].events & EPOLLIN) {
            pool->addTask(std::bind(&httpConn::processRead, &this->clients[socketfd]));
        }
        else if (events[i].events & EPOLLOUT) {
            pool->addTask(std::bind(&httpConn::processWrite, &this->clients[socketfd]));
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
    epoll_event events[maxEventNumber];
    int efd = epoll_create(8);
    if (efd == -1) {
        perror("epoll_create fail.");
        return;
    }
    addfd(efd, listenfd);

    while (true) {
        int ret = epoll_wait(efd, events, maxEventNumber, -1);
        if (ret < 0) {
            // // 被调试打断
            // if (errno == EINTR)
            //     continue;
            perror("epoll_wait fail.");
            return;
        }
        epollHandler(events, ret, efd, listenfd);
    }
}
