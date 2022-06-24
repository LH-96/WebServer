#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <sys/socket.h>
#include <string.h>
#include <vector>

void work(int i) {
    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in caddr;
    caddr.sin_family = AF_INET;
    caddr.sin_port = htons(1024);
    inet_pton(AF_INET, "127.0.0.1", &caddr.sin_addr.s_addr);
    socklen_t caddrLen = sizeof(caddr);
    int ret = connect(cfd, (struct sockaddr*)&caddr, caddrLen);
    if (ret == -1) {
        perror("connect fail.");
        exit(1);
    }

    char buf[100];
    for (int i = 0; i < 1; i++) {
        bzero(buf, sizeof(buf));
        sprintf(buf, "no %d loop.\n", i);
        int ret = send(cfd, buf, sizeof(buf), 0);
        sleep(1);
        printf("thread %d send %d\n", i, ret);
    }
    close(cfd);
}

int main() {
    std::vector<std::thread> vec(1021);
    for (int i = 0; i < vec.size(); i++) {
        vec.at(i) = std::thread(work, i);
    }
    for (int i = 0; i < vec.size(); i++) {
        vec.at(i).join();
    }
}
