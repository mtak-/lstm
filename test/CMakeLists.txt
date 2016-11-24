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

add_executable(thread_data_synchronization thread_data_synchronization.cpp)
target_link_libraries(thread_data_synchronization)
add_test(test.thread_data_synchronization thread_data_synchronization)

add_executable(exception exception.cpp)
target_link_libraries(exception)
add_test(test.exception exception)

add_executable(destructor_transaction destructor_transaction.cpp)
target_link_libraries(destructor_transaction)
add_test(test.destructor_transaction destructor_transaction)