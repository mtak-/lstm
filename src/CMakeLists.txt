add_executable(main main.cpp playground.cpp)
target_link_libraries(main urcu-signal)