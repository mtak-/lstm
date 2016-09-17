cmake_minimum_required(VERSION 3.1)

project(LSTM CXX)

enable_testing()
include(CTest)

include(WriteCompilerDetectionHeader)
write_compiler_detection_header(
  FILE "${CMAKE_SOURCE_DIR}/include/lstm/detail/thread_local.hpp"
  PREFIX LSTM
  COMPILERS GNU Clang AppleClang MSVC
  FEATURES cxx_thread_local
)

include_directories(include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -ftemplate-backtrace-limit=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-variable")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DLSTM_LOG_TRANSACTIONS -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

add_subdirectory(src)
add_subdirectory(test)