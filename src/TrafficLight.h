#ifndef TRAFFICLIGHT_H
#define TRAFFICLIGHT_H

#include <atomic>
#include <random>
#include <future>
#include "TrafficObject.h"
#include "MessageQueue.h"

// forward declarations to avoid include cycle
class Vehicle;

enum TrafficLightPhase
{
    red,
    green
};

class TrafficLight : public TrafficObject
{
public:
    // constructor / desctructor
    TrafficLight();

    // getters / setters
    TrafficLightPhase getCurrentPhase();

    // typical behaviour methods
    void waitForGreen();
    void simulate();

private:
    // typical behaviour methods
    void cycleThroughPhases();
    std::atomic<TrafficLightPhase> _currentPhase;

    std::shared_ptr<MessageQueue<TrafficLightPhase>> _message_queue;
};

#endif
