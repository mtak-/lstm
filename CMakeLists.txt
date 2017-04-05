cmake_minimum_required(VERSION 3.1)

project(LSTM)

enable_testing()
include(CTest)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -march=native")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG -fvisibility=hidden")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer -fno-rtti -flto")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -ftemplate-backtrace-limit=0 -march=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DLSTM_PERF_STATS_ON -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -fvisibility=hidden")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fomit-frame-pointer")
set(STATSD_LIST_PATH "${CMAKE_CURRENT_LIST_DIR}/statsd-c-client/CMakeLists.txt")
if (EXISTS "${STATSD_LIST_PATH}")
    set(STATSD TRUE)
    set(STATSD_INCLUDE_DIR "statsd-c-client")
    set(STATSD_SUBDIR "statsd-c-client")
    add_subdirectory(${STATSD_SUBDIR})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLSTM_STATSD_AVAILABLE")
endif()

function(conditional_link_statsd target)
    if (STATSD)
        target_link_libraries(${target} statsd_client)
    endif()
endfunction()

include_directories(include ${STATSD_INCLUDE_DIR})

add_subdirectory(test)