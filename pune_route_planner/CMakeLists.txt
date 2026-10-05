cmake_minimum_required(VERSION 3.14)
project(PuneTouristRoutePlanner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra -pedantic -O2)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Common planner & utility sources
set(COMMON_SOURCES
    src/planner/Models.cpp
    src/planner/Scoring.cpp
    src/planner/Dijkstra.cpp
    src/planner/Alternatives.cpp
    src/planner/Tour.cpp
    src/planner/Dynamic.cpp
    src/util/Json.cpp
)

# Main Web Server Executable
add_executable(pune_route_planner
    src/main.cpp
    ${COMMON_SOURCES}
)

if(WIN32)
    target_link_libraries(pune_route_planner PRIVATE ws2_32)
endif()

# Find Threads library for cpp-httplib
find_package(Threads REQUIRED)
target_link_libraries(pune_route_planner PRIVATE Threads::Threads)

# Unit / Integration Tests Executable
add_executable(test_all
    tests/test_all.cpp
    ${COMMON_SOURCES}
)

enable_testing()
add_test(NAME PuneRoutePlannerTests COMMAND test_all "${CMAKE_CURRENT_SOURCE_DIR}/data/pune_demo.json")
