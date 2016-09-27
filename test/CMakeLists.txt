add_executable(var var.cpp)
target_link_libraries(var urcu-signal)
add_test(test.var var)

add_executable(transaction transaction.cpp)
target_link_libraries(transaction urcu-signal)
add_test(test.transaction transaction)

add_executable(dining_philosophers dining_philosophers.cpp)
target_link_libraries(dining_philosophers urcu-signal)
add_test(test.dining_philosophers dining_philosophers)

add_executable(nesting nesting.cpp)
target_link_libraries(nesting urcu-signal)
add_test(test.nesting nesting)

add_executable(transfer_funds transfer_funds.cpp)
target_link_libraries(transfer_funds urcu-signal)
add_test(test.transfer_funds transfer_funds)