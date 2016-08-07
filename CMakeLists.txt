cmake_minimum_required(VERSION 2.8)

project(FCC_TUPLE CXX)

enable_testing()
include(CTest)

include_directories(include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -ftemplate-backtrace-limit=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-variable")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_subdirectory(src)
add_subdirectory(test)