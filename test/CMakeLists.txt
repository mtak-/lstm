add_executable(var var.cpp)
target_link_libraries(var)
add_test(test.var var)

add_executable(transaction transaction.cpp)
target_link_libraries(transaction)
add_test(test.transaction transaction)

add_executable(dining_philosophers dining_philosophers.cpp)
target_link_libraries(dining_philosophers)
add_test(test.dining_philosophers dining_philosophers)

add_executable(nesting nesting.cpp)
target_link_libraries(nesting)
add_test(test.nesting nesting)

add_executable(transfer_funds transfer_funds.cpp)
target_link_libraries(transfer_funds)
add_test(test.transfer_funds transfer_funds)

add_executable(list list.cpp)
target_link_libraries(list)
add_test(test.list list)