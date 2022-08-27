#include "timer.h"

void heapTimer::swapNode(size_t i, size_t j) {
    std::swap(heap[i], heap[j]);
    idMap[heap[i].id] = i;
    idMap[heap[j].id] = j;
} 

void heapTimer::addTimer(int id, int timeout, const CALLBACK& cb) {
    size_t i;
    if(idMap.count(id) == 0) {
        // 插入新节点
        i = ++this->heapSize;
        auto tmpNode = timeNode{id, CLOCK::now() + MS(timeout), cb};
        heap.push_back(timeNode{id, TIMEPOINT{}, cb});
        for (; tmpNode < heap[i/2]; i /= 2) {
            heap[i] = heap[i/2];
            idMap[heap[i/2].id] = i;
        }
        heap[i] = tmpNode;
        idMap[tmpNode.id] = i;
    } 
    else {
        // 节点存在，说明fd已重新利用，直接调整节点的timeout
        i = idMap[id];
        heap[i].expires = CLOCK::now() + MS(timeout);
        heap[i].cb = cb;
        auto tmpNode = heap[i];
        size_t child;
        for (; i*2 <= heapSize; i = child) {
            child = i*2;
            if (child != heapSize && heap[child+1] < heap[child])
                child++;
            if (heap[child] < tmpNode) {
                heap[i] = heap[child];
                idMap[heap[child].id] = i;
            }
            else
                break;
        }
        heap[i] = tmpNode;
        idMap[tmpNode.id] = i;
    }
}

void heapTimer::delTop(size_t index) {
    swapNode(index, heapSize);
    idMap.erase(heap.back().id);
    heap.pop_back();
    heapSize--;

    if (heapSize == 0)
        return;

    auto tmpNode = heap[index];
    size_t i, child;
    for (i = index; i*2 <= heapSize; i = child) {
        child = i*2;
        if (child != heapSize && heap[child+1] < heap[child])
            child++;
        if (heap[child] < tmpNode) {
            heap[i] = heap[child];
            idMap[heap[child].id] = i;
        }
        else
            break;
    }
    heap[i] = tmpNode;
    idMap[tmpNode.id] = i;
}

void heapTimer::adjustTimer(int id, int timeout) {
    heap[idMap[id]].expires = CLOCK::now() + MS(timeout);

    size_t i = idMap[id];
    auto tmpNode = heap[i];
    size_t child;
    for (; i*2 <= heapSize; i = child) {
        child = i*2;
        if (child != heapSize && heap[child+1] < heap[child])
            child++;
        if (heap[child] < tmpNode) {
            heap[i] = heap[child];
            idMap[heap[child].id] = i;
        }
        else
            break;
    }
    heap[i] = tmpNode;
    idMap[tmpNode.id] = i;
}

void heapTimer::tick() {
    // 处理超时节点
    if(heapSize == 0) {
        return;
    }
    while(heapSize != 0) {
        timeNode node = heap[1];
        if(std::chrono::duration_cast<MS>(node.expires - CLOCK::now()).count() > 0) { 
            break; 
        }
        node.cb();
        delTop(1);
    }
}

int heapTimer::getNextTick() {
    tick();
    int res = -1;
    if(heapSize != 0) {
        res = std::chrono::duration_cast<MS>(heap[1].expires - CLOCK::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}
