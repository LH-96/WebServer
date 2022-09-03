# WebServer
使用C++实现的Web服务器

## 功能
* 使用IO多路复用技术epoll，使用边缘触发和非阻塞IO，使用Reactor模式；
* 使用多线程充分利用多核CPU，并利用C++11并发支持库实现线程池，避免线程频繁创建销毁的开销；
* 使用状态机解析HTTP请求报文，支持解析GET和POST的请求，支持长/短连接，管线化请求，支持解析HTTP Range请求，实现文件下载和断点续存；
* 使用unordered_map和vector实现基于小根堆的定时器，关闭超时的非活动连接；
* 利用单例模式与阻塞队列实现异步的日志系统，记录服务器运行状态；
* 使用Webbench压力测试，在6核主频2.9GHz，内存16GB的主机上，可以达到60000+QPS。

## 运行环境
* Linux
* C++14

## 项目启动
```bash
mkdir build
cd build
cmake ..
make
./WebServer
```

## WeBbench压力测试
![image-webbench](https://github.com/LH-96/WebServer/blob/main/root/webbench_test.png)  

```bash
./WebBench/webbench -c 1000 -t 30 -2 http://127.0.0.1:10240/
```

测试环境: Ubuntu 20.04 LTS(on the Windows Subsystem for Linux) cpu:i5-10400 内存:16G 
QPS 60000+

## 参考

[@qinguoyi](https://github.com/qinguoyi/TinyWebServer)
[@markparticle](https://github.com/markparticle/WebServer)
[@linyacool](https://github.com/linyacool/WebServer)