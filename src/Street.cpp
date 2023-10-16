#include <iostream>
#include <stdexcept>
#include "Vehicle.h"
#include "Intersection.h"
#include "Street.h"


Street::Street()
{
    _type = ObjectType::objectStreet;
    _length = 1000.0; // in m
}

void Street::setInIntersection(std::shared_ptr<Intersection> in)
{
    if (!in)
        throw std::invalid_argument("Street::setInIntersection: intersection must not be null");

    _interIn = in;
    in->addStreet(get_shared_this()); // add this street to list of streets connected to the intersection
}

void Street::setOutIntersection(std::shared_ptr<Intersection> out)
{
    if (!out)
        throw std::invalid_argument("Street::setOutIntersection: intersection must not be null");

    _interOut = out;
    out->addStreet(get_shared_this()); // add this street to list of streets connected to the intersection
}
