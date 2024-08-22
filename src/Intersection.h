#ifndef INTERSECTION_H
#define INTERSECTION_H

#include <atomic>
#include <cstddef>
#include <vector>
#include <future>
#include <mutex>
#include <memory>
#include "TrafficLight.h"
#include "TrafficObject.h"
#include "WorkerState.h"

// forward declarations to avoid include cycle
class Street;
class Vehicle;

// auxiliary class to queue and dequeue waiting vehicles in a thread-safe manner
class WaitingVehicles
{
public:
    // getters / setters
    std::size_t getSize();

    // typical behaviour methods
    bool pushBack(std::shared_ptr<Vehicle> vehicle, std::promise<void> &&promise);
    void permitEntryToFirstInQueue();
    void shutdown();

private:
    std::vector<std::shared_ptr<Vehicle>> _vehicles; // list of all vehicles waiting to enter this intersection
    std::vector<std::promise<void>> _promises;       // list of associated promises
    std::mutex _mutex;
    bool _closed = false;
};

class Intersection : public TrafficObject
{
public:
    // constructor / desctructor
    Intersection();
    ~Intersection();

    // getters / setters
    void setIsBlocked(bool isBlocked);

    // typical behaviour methods
    bool addVehicleToQueue(std::shared_ptr<Vehicle> vehicle);
    void addStreet(std::shared_ptr<Street> street);
    std::vector<std::shared_ptr<Street>> queryStreets(std::shared_ptr<Street> incoming); // return pointer to current list of all outgoing streets
    void simulate();
    void vehicleHasLeft(std::shared_ptr<Vehicle> vehicle);
    bool trafficLightIsGreen();
    void shutdown();

private:
    // typical behaviour methods
    void processVehicleQueue();

    // private members
    std::vector<std::shared_ptr<Street>> _streets; // list of all streets connected to this intersection
    mutable std::mutex _streetsMutex;
    WaitingVehicles _waitingVehicles;              // list of all vehicles and their associated promises waiting to enter the intersection
    std::atomic<bool> _isBlocked;                  // flag indicating wether the intersection is blocked by a vehicle
    TrafficLight _trafficLight;
    WorkerState _workerState;
};

#endif
