#include <iostream>
#include <random>
#include "TrafficLight.h"

/* Implementation of class "TrafficLight" */

TrafficLight::TrafficLight()
{
    _currentPhase = TrafficLightPhase::red;
    _message_queue = std::make_shared<MessageQueue<TrafficLightPhase>>();
}

TrafficLight::~TrafficLight()
{
    // Stop *and* join the phase-cycling thread here, so a TrafficLight can
    // always be destroyed safely even if the owner never called shutdown()
    // explicitly. Joining in the base destructor would be too late: the
    // thread reads _currentPhase, _message_queue and _workerState, which are
    // destroyed before ~TrafficObject() runs.
    shutdown();
    joinThreads();
}

void TrafficLight::waitForGreen()
{
    while (true)
    {
        if (_message_queue.get()->receive() == TrafficLightPhase::green)
        {
            return;
        }
    }
}

TrafficLightPhase TrafficLight::getCurrentPhase()
{
    return _currentPhase;
}

void TrafficLight::simulate()
{
    threads.emplace_back(std::thread(&TrafficLight::cycleThroughPhases, this));
}

void TrafficLight::shutdown()
{
    _workerState.stop();
}

void TrafficLight::cycleThroughPhases()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(4000, 6000);

    double cycleDuration = distr(gen);
    std::chrono::time_point<std::chrono::system_clock> lastUpdate;

    lastUpdate = std::chrono::system_clock::now();
    while (_workerState.is_running())
    {
        _workerState.wait_for_stop(std::chrono::milliseconds(1));

        long timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - lastUpdate).count();
        if (timeSinceLastUpdate >= cycleDuration)
        {
            if (_currentPhase == TrafficLightPhase::red)
            {
                _currentPhase = TrafficLightPhase::green;
            }
            else
            {
                _currentPhase = TrafficLightPhase::red;
            }

            TrafficLightPhase message = _currentPhase;
            auto ft = std::async(std::launch::async,
                                 &MessageQueue<TrafficLightPhase>::send,
                                 _message_queue,
                                 std::move(message));
            ft.wait();

            lastUpdate = std::chrono::system_clock::now();
            cycleDuration = distr(gen);
        }
    }
}
