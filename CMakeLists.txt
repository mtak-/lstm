cmake_minimum_required(VERSION 2.8)

project(FCC_TUPLE CXX)

enable_testing()
include(CTest)

include_directories(include)

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -ftemplate-backtrace-limit=0 -fconcepts")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-variable")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

add_subdirectory(src)
add_subdirectory(test)