#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/tcp.h>

bool isTCPConn(int sock) {
	struct tcp_info info; 
    socklen_t len = sizeof(info);
	getsockopt(sock, IPPROTO_TCP, TCP_INFO, &info, &len);
    return info.tcpi_state == TCP_ESTABLISHED ? true : false;
}

int main() {
    struct sockaddr_in caddr;
    memset(&caddr, 0, sizeof(struct sockaddr_in));
    caddr.sin_family = AF_INET;
    caddr.sin_port = htons(10240);
    inet_pton(AF_INET, "127.0.0.1", (void*)&caddr.sin_addr);
    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    int ret = connect(cfd, (struct sockaddr*)&caddr, sizeof(caddr));
    char meg[200] = "get / http/1.1\r\nConnection:keep-alive\r\n\r\n";
    send(cfd, meg, strlen(meg), 0);
    char rbuf[1024];
    ret = read(cfd, rbuf, sizeof(rbuf));
    printf("%s\n", rbuf);
    sleep(5);
    if (isTCPConn(cfd))
        close(cfd);
    return 0;
}
