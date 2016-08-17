#include <lstm/transaction.hpp>
#include <lstm/var.hpp>

#include "simple_test.hpp"

LSTM_TEST_BEGIN
    struct transaction_tester {
        static int run_tests();
    };
LSTM_TEST_END

int main() {
    return lstm::test::transaction_tester::run_tests();
}

int lstm::test::transaction_tester::run_tests() {
    // init
    {
        CHECK(transaction<>::clock<> == 0u);
        transaction<> tx{{}};
        CHECK(transaction<>::clock<> == 0u);
        CHECK(tx.write_set.size() == 0u);
        CHECK(tx.read_set.size() == 0u);
    }

    // locking (private but important)
    {
        var<int> v{0};
        transaction<> tx{{}};
        tx.read_version = 0;

        CHECK(v.version_lock == 0u);

        word version_buf;
        {
            version_buf = 0u;
            CHECK(tx.lock(version_buf, v) == true);
            CHECK(v.version_lock == 1u);

            tx.unlock(v);
            CHECK(v.version_lock == 0u);
        }
        {
            version_buf = 0u;
            CHECK(tx.lock(version_buf, v) == true);
            CHECK(v.version_lock == 1u);

            version_buf = 0u;
            CHECK(tx.lock(version_buf, v) == false);
            CHECK(v.version_lock == 1u);

            tx.unlock(v);
            CHECK(v.version_lock == 0u);
        }
        {
            v.version_lock = 3000u;
            CHECK(tx.lock(version_buf, v) == false);
        }

        CHECK(tx.read_set.size() == 0u);
    }

    // loads
    {
        var<int> x{42};
        transaction<> tx0{{}};
        tx0.read_version = 0;

        CHECK(tx0.load(x) == 42);
        CHECK(tx0.read_set.size() == 1u);
        CHECK(tx0.read_set.count(&x) == 1u);

        ++x.unsafe();

        CHECK(tx0.load(x) == 43);
        CHECK(tx0.read_set.size() == 1u);
        CHECK(tx0.read_set.count(&x) == 1u);

        transaction<> tx1{{}};
        tx1.read_version = 0;
        CHECK(tx1.load(x) == 43);
        CHECK(tx1.read_set.size() == 1u);
        CHECK(tx1.read_set.count(&x) == 1u);

        CHECK(tx0.load(x) == 43);
        CHECK(tx0.read_set.size() == 1u);
        CHECK(tx0.read_set.count(&x) == 1u);
    }

    // stores
    {
        var<int> x{42};
        transaction<> tx0{{}};
        tx0.read_version = 0;

        tx0.store(x, 43);
        CHECK(tx0.write_set.size() == 1u);
        CHECK(tx0.write_set.count(&x) == 1u);
        CHECK(reinterpret_cast<int&>(tx0.write_set.at(&x)) == 43);
        
        CHECK(tx0.load(x) == 43);
        CHECK(x.unsafe() == 42);

        transaction<> tx1{{}};
        tx1.read_version = 0;

        CHECK(tx1.load(x) == 42);
        tx1.store(x, 44);
        CHECK(tx1.write_set.size() == 1u);
        CHECK(tx1.write_set.count(&x) == 1u);
        CHECK(reinterpret_cast<int&>(tx1.write_set.at(&x)) == 44);

        CHECK(tx1.load(x) == 44);
        CHECK(tx0.load(x) == 43);
        CHECK(x.unsafe() == 42);

        tx0.cleanup();
        tx1.cleanup();
    }
    return test_result();
}
