add_executable(var var.cpp)
add_test(test.var var)

add_executable(transaction transaction.cpp)
add_test(test.transaction transaction)

add_executable(dining_philosophers dining_philosophers.cpp)
add_test(test.dining_philosophers dining_philosophers)

add_executable(nesting nesting.cpp)
add_test(test.nesting nesting)

add_executable(transfer_funds transfer_funds.cpp)
add_test(test.transfer_funds transfer_funds)

add_executable(list list.cpp)
add_test(test.list list)

add_executable(thread_data_synchronization thread_data_synchronization.cpp)
add_test(test.thread_data_synchronization thread_data_synchronization)

add_executable(exception exception.cpp)
add_test(test.exception exception)

add_executable(destructor_transaction destructor_transaction.cpp)
add_test(test.destructor_transaction destructor_transaction)

add_executable(delete_var_and_modify delete_var_and_modify.cpp)
add_test(test.delete_var_and_modify delete_var_and_modify)

add_executable(rbtree rbtree.cpp)
add_test(test.rbtree rbtree)