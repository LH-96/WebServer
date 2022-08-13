#include "timer.h"

inline
bool operator<(const timeNode& lhd, const timeNode& rhd) {
    return lhd.expire < rhd.expire ? true : false;
}

time_t timer::getTime() {
    auto timeNow = std::chrono::time_point_cast<MS>(CLOCK::now());
    auto duration = std::chrono::duration_cast<MS>(timeNow.time_since_epoch());
    return duration.count();
}

time_t timer::timeToTrig() {
    auto beg = timeMap.begin();
    if (beg == timeMap.end())
        return -1;
    auto distance = beg->expire - getTime();
    return distance > 0 ? distance : 0;
}

bool timer::isTCPConn(int sock) {
	struct tcp_info info; 
    socklen_t len = sizeof(info);
	getsockopt(sock, IPPROTO_TCP, TCP_INFO, &info, &len);
    return info.tcpi_state == TCP_ESTABLISHED ? true : false;
}

void timer::addTimer(int cfd, time_t expire, std::function<void()> func) {
    fdToNode.emplace(cfd, timeNode(cfd, getTime()+expire, func));
    timeMap.emplace(cfd, getTime()+expire, func);
}

void timer::resetTimer(int cfd, time_t expireNew) {
    auto node = timeMap.find(fdToNode[cfd]);
    if (node != timeMap.end()) {
        auto func = node->func;
        timeMap.erase(node);
        fdToNode.erase(cfd);
        addTimer(cfd, expireNew, func);
    }
}

bool timer::checkTimer() {
    auto beg = timeMap.begin();
    if (beg != timeMap.end() && beg->expire <= getTime()) {
        if (isTCPConn(beg->cfd)) {
            beg->func();
        }
        timeMap.erase(beg);
        fdToNode.erase(beg->cfd);
        return true;
    }
    return false;
}
