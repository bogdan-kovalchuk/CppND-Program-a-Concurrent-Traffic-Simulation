#include <algorithm>
#include <iostream>
#include <chrono>
#include "TrafficObject.h"

// init static variable
std::atomic<int> TrafficObject::_idCnt{0};

std::mutex TrafficObject::_mtx;

void TrafficObject::setPosition(double x, double y)
{
    std::lock_guard<std::mutex> lock(_positionMtx);
    _posX = x;
    _posY = y;
}

void TrafficObject::getPosition(double &x, double &y) const
{
    std::lock_guard<std::mutex> lock(_positionMtx);
    x = _posX;
    y = _posY;
}

TrafficObject::TrafficObject()
{
    _type = ObjectType::noObject;
    _id = _idCnt.fetch_add(1, std::memory_order_relaxed);
    _posX = 0.0;
    _posY = 0.0;
}

void TrafficObject::joinThreads()
{
    std::for_each(threads.begin(), threads.end(), [](std::thread &t) {
        if (t.joinable())
            t.join();
    });
}

TrafficObject::~TrafficObject()
{
    // Safety net for objects whose derived destructor did not already join
    // (e.g. plain TrafficObjects). Derived classes owning a worker thread
    // must still join in their own destructor -- see joinThreads().
    joinThreads();
}
