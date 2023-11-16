# CPPND: Program a Concurrent Traffic Simulation

<img src="data/traffic_simulation.gif"/>

This is the project for the fourth course in the [Udacity C++ Nanodegree Program](https://www.udacity.com/course/c-plus-plus-nanodegree--nd213): Concurrency.

## Dependencies for Running Locally
* cmake >= 2.8
  * All OSes: [click here for installation instructions](https://cmake.org/install/)
* make >= 4.1 (Linux, Mac), 3.81 (Windows)
  * Linux: make is installed by default on most Linux distros
  * Mac: [install Xcode command line tools to get make](https://developer.apple.com/xcode/features/)
  * Windows: [Click here for installation instructions](http://gnuwin32.sourceforge.net/packages/make.htm)
* OpenCV >= 4.1
  * The OpenCV 4.1.0 source code can be found [here](https://github.com/opencv/opencv/tree/4.1.0)
* gcc/g++ >= 5.4
  * Linux: gcc / g++ is installed by default on most Linux distros
  * Mac: same deal as make - [install Xcode command line tools](https://developer.apple.com/xcode/features/)
  * Windows: recommend using [MinGW](http://www.mingw.org/)

## Basic Build Instructions

1. Clone this repo.
2. Make a build directory in the top level directory: `mkdir build && cd build`
3. Compile: `cmake .. && make`
4. Run it: `./traffic_simulation`.

## Running the Unit Tests

The `tests/` directory contains a standalone CMake project with unit and
concurrency tests for the queue, worker-state, and traffic-object classes in
`src/`. Unlike the main `traffic_simulation` target, it does not depend on
OpenCV, so it can be built and run in environments where OpenCV 4.1 is not
installed (e.g. to validate concurrency/shutdown-safety fixes without a full
GUI toolchain):

```
cd tests
mkdir build && cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

Note: `Graphics.cpp` and `TrafficSimulator-Final.cpp` (the OpenCV-dependent
GUI and entry point) are not exercised by this test suite, since they require
OpenCV to compile. All other production sources in `src/` (`TrafficObject`,
`TrafficLight`, `Street`, `Vehicle`, `Intersection`, `MessageQueue`,
`FifoMessageQueue`, `WorkerState`) are OpenCV-free and are compiled directly
into the test executables, so the tests exercise the real classes rather than
reimplementations.