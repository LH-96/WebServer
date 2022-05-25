#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <iostream>
#include <strings.h>
#include <string.h>

// 给每个连接的client，都设置一个结构保存对应的fd和addr
struct connfdInfo {
    int connfd;
    struct sockaddr_in caddr;
};

// 子线程操作
void* saysomething(void *arg) {
    struct connfdInfo *cfdInfo = (struct connfdInfo*)arg;

    char strIP[INET_ADDRSTRLEN];
    printf("client ip : %s , client port : %d\n", 
            inet_ntop(AF_INET, &(*cfdInfo).caddr.sin_addr, strIP, sizeof(strIP)), 
            ntohs((*cfdInfo).caddr.sin_port));
    char buf[1024];
    bzero((void*)buf, sizeof(buf));
    strcpy(buf, "hello\n");
    send((*cfdInfo).connfd, buf, sizeof(buf), 0);

    // 关闭fd，释放对应结构体的内存
    close((*cfdInfo).connfd);
    delete cfdInfo;

    return (void*)0;
}

int main(int, char**) {

    int ret; // 检查函数的返回值

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == 0) {
        perror("listenfd create fail.");
        return -1;
    }

    int opt = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
    if (ret != 0) {
        perror("port reuse set fail.");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1024);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret != 0) {
        perror("bind fail.");
        return -1;
    }

    ret = listen(listenfd, 128);
    if (ret != 0) {
        perror("listen fail.");
        return -1;
    }

    struct sockaddr_in caddr;
    bzero((void*)&caddr, sizeof(caddr));
    socklen_t caddrLen;
    int connfd;
    struct connfdInfo *cfdInfo;

    pthread_t tid;

    while (true) {
        caddrLen = sizeof(caddr);
        connfd = accept(listenfd, (sockaddr *)&caddr, &caddrLen);
        if (connfd == -1) {
            perror("accept fail.");
            return -1;
        }
        else {
            //每个新连接都分配新的内存存储fd和addr，可防止accept在子线程执行前接收新连接而导致fd被改变。
            //内存在子线程中回收
            cfdInfo = new struct connfdInfo;
            cfdInfo->caddr = caddr;
            cfdInfo->connfd = connfd;
            pthread_create(&tid, NULL, saysomething, (void*)cfdInfo);
            pthread_detach(tid);
        }
    }

    return 0;
}
