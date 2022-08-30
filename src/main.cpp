#include "webserver.h"

int main() {
    webserver server("127.0.0.1", "10240", 8, 20000, // ip地址，端口号，线程数量，定时时间
                     0, 1, 4096, 600000); // log状态(0开1关)，log方式(0同步1异步)，log缓冲区大小，单个log文件最大行数
    server.run();
}
