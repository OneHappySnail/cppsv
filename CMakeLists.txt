cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(csvpp)

# Testing
option(DO_TEST "Build and run tests" OFF)
if(DO_TEST)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(
        csvpp
        csvpp/csvpp.test.cpp
    )

    target_link_libraries(
        csvpp
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(csvpp)
else()
    add_library(csvpp INTERFACE csvpp/csvpp.hpp)
endif(DO_TEST)
