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

bool TrafficLight::waitForGreen()
{
    try
    {
        while (true)
        {
            if (_message_queue->receive() == TrafficLightPhase::green)
            {
                return true;
            }
        }
    }
    catch (const QueueClosedException &)
    {
        return false;
    }
}

TrafficLightPhase TrafficLight::getCurrentPhase()
{
    return _currentPhase;
}

void TrafficLight::simulate()
{
    if (!_workerState.is_running())
        return;

    bool expected = false;
    if (!_simulationStarted.compare_exchange_strong(expected, true))
        return;

    threads.emplace_back(std::thread(&TrafficLight::cycleThroughPhases, this));
}

void TrafficLight::shutdown()
{
    _message_queue->shutdown();
    _workerState.stop();
}

void TrafficLight::cycleThroughPhases()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(4000, 6000);

    int cycleDuration = distr(gen);
    auto lastUpdate = std::chrono::steady_clock::now();
    while (_workerState.is_running())
    {
        _workerState.wait_for_stop(std::chrono::milliseconds(1));

        const auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastUpdate).count();
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
            if (!_message_queue->try_send(std::move(message)))
                return;

            lastUpdate = std::chrono::steady_clock::now();
            cycleDuration = distr(gen);
        }
    }
}
