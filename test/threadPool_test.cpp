#include "threadpool.h"

int main() {
    // 测试线程池是否正常开启和关闭
    {
        std::shared_ptr<threadPool> pool = std::make_shared<threadPool>(8);
        sleep(10);
    }
    sleep(10);
}
