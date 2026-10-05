# Pune Tourist Route Planner

> **A Personalized Multi-Objective Route Planning System with Custom Data Structures in C++17 and Interactive Browser Frontend.**

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/Build-CMake%20%7C%20g%2B%2B-brightgreen.svg)](https://cmake.org)
[![Status](https://img.shields.io/badge/Course_Project-Data_Structures-orange.svg)]()

---

## Quick Start

```bash
cd pune_route_planner

# Option 1: One-line g++ build
g++ -std=c++17 -Wall -Wextra -O2 src/main.cpp src/planner/Models.cpp src/planner/Scoring.cpp src/planner/Dijkstra.cpp src/planner/Alternatives.cpp src/planner/Tour.cpp src/planner/Dynamic.cpp src/util/Json.cpp -lws2_32 -o pune_route_planner.exe
./pune_route_planner.exe --port 8080

# Option 2: CMake build
cmake -B build -S .
cmake --build build --config Release
./build/pune_route_planner.exe
```

Open your browser to: **`http://127.0.0.1:8080`**

To run tests:
```bash
cd pune_route_planner
g++ -std=c++17 -Wall -Wextra -O2 tests/test_all.cpp src/planner/Models.cpp src/planner/Scoring.cpp src/planner/Dijkstra.cpp src/planner/Alternatives.cpp src/planner/Tour.cpp src/planner/Dynamic.cpp src/util/Json.cpp -o test_all.exe
./test_all.exe data/pune_demo.json
```

See [`pune_route_planner/README.md`](pune_route_planner/README.md) for full project documentation, algorithm details, and Data Structure viva questions.
