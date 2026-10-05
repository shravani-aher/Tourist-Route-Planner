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

# Tests must remain active in Release builds (assert is the legacy harness).
if(MSVC)
    target_compile_options(test_all PRIVATE /UNDEBUG)
else()
    target_compile_options(test_all PRIVATE -UNDEBUG)
endif()
option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(ENABLE_SANITIZERS AND NOT MSVC)
    foreach(target pune_route_planner test_all)
        target_compile_options(${target} PRIVATE -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()
enable_testing()
add_test(NAME PuneRoutePlannerTests COMMAND test_all "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/pune_demo.json")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME HttpRegressions COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_api.py"
    "$<TARGET_FILE:pune_route_planner>"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/pune_demo.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/web")
set_tests_properties(HttpRegressions PROPERTIES TIMEOUT 60)
