#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>

void saysomething(int connfd, const char *message) {
    char buf[1024];
    bzero((void*)buf, sizeof(buf));
    strcpy(buf, message);
    send(connfd, buf, sizeof(buf), 0);
}

int main(int, char**) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == 0) {
        printf("listenfd create fail.\n");
        return -1;
    }

    int opt = 1;
    int portReuse = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
    if (portReuse != 0) {
        printf("portreuse fail.\n");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1024);
    addr.sin_addr.s_addr = INADDR_ANY;

    int bindRes = bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    if (bindRes != 0) {
        printf("bind fail.\n");
        return -1;
    }

    int listenRes = listen(listenfd, 128);
    if (listenRes != 0) {
        printf("listen fail.\n");
        return -1;
    }

    struct sockaddr caddr;
    socklen_t caddrLen;
    bzero((void*)&caddr, sizeof(caddr));
    while (true) {
        int connfd = accept(listenfd, (sockaddr *)&caddr, &caddrLen);
        if (connfd == -1) {
            printf("accept fail.\n");
            return -1;
        }
        else {
            saysomething(connfd, "hello world.\n");
            close(connfd);
        }
    }
    close(listenfd);
    return 0;
}
