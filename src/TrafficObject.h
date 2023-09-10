#ifndef TRAFFICOBJECT_H
#define TRAFFICOBJECT_H

#include <vector>
#include <thread>
#include <mutex>

enum ObjectType
{
    noObject,
    objectVehicle,
    objectIntersection,
    objectStreet,
};

class TrafficObject
{
public:
    // constructor / desctructor
    TrafficObject();
    virtual ~TrafficObject();

    // getter and setter
    int getID() { return _id; }
    void setPosition(double x, double y);
    void getPosition(double &x, double &y);
    ObjectType getType() { return _type; }

    // typical behaviour methods
    virtual void simulate(){};

protected:
    // Joins every thread this object has launched. Idempotent: threads that
    // have already been joined are skipped, so it is safe to call more than
    // once.
    //
    // Every derived class that launches a thread touching its own members
    // must call this from its own destructor (right after signalling
    // shutdown). A derived destructor runs *before* the derived members are
    // destroyed, which in turn happens before ~TrafficObject() runs -- so
    // relying on the base destructor alone to join would let a still-running
    // worker touch members that have already been destroyed.
    void joinThreads();

    ObjectType _type;                 // identifies the class type
    int _id;                          // every traffic object has its own unique id
    double _posX, _posY;              // vehicle position in pixels
    std::vector<std::thread> threads; // holds all threads that have been launched within this object
    static std::mutex _mtx;           // mutex shared by all traffic objects for protecting cout 

private:
    static int _idCnt; // global variable for counting object ids
};

#endif