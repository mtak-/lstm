#include <lstm/transaction.hpp>
#include <lstm/detail/transaction_impl.hpp>

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
    using tx_t = detail::transaction_impl<std::allocator<int>, 4, 4, 4>;
    // init
    {
        CHECK(default_domain().get_clock() == 0u);
        tx_t tx{nullptr, {}};
        CHECK(tx.domain().get_clock() == 0u);
        CHECK(tx.write_set.size() == 0u);
        CHECK(tx.read_set.size() == 0u);
    }
    
    // locking (private but important)
    {
        var<int> v{0};
        tx_t tx{nullptr, {}};
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
        tx_t tx0{nullptr, {}};
        tx0.read_version = 0;
    
        CHECK(tx0.load(x) == 42);
        CHECK(tx0.read_set.size() == 1u);
        CHECK(tx0.find_read_set(x) != std::end(tx0.read_set));
    
        ++x.unsafe();
    
        CHECK(tx0.load(x) == 43);
        CHECK(tx0.read_set.size() == 1u);
        CHECK(tx0.find_read_set(x) != std::end(tx0.read_set));
    
        tx_t tx1{nullptr, {}};
        tx1.read_version = 0;
        CHECK(tx1.load(x) == 43);
        CHECK(tx1.read_set.size() == 1u);
        CHECK(tx0.find_read_set(x) != std::end(tx0.read_set));
    
        CHECK(tx0.load(x) == 43);
        CHECK(tx0.read_set.size() == 1u);
        CHECK(tx0.find_read_set(x) != std::end(tx0.read_set));
        
        tx0.cleanup();
        tx1.cleanup();
    }
    
    // stores
    {
        var<int> x{42};
        tx_t tx0{nullptr, {}};
        tx0.read_version = 0;
    
        tx0.store(x, 43);
        CHECK(tx0.write_set.size() == 1u);
        CHECK(tx0.find_write_set(x).success());
        CHECK(reinterpret_cast<int&>(tx0.write_set[0].pending_write()) == 43);
    
        CHECK(tx0.load(x) == 43);
        CHECK(x.unsafe() == 42);
    
        tx_t tx1{nullptr, {}};
        tx1.read_version = 0;
    
        CHECK(tx1.load(x) == 42);
        tx1.store(x, 44);
        CHECK(tx1.write_set.size() == 1u);
        CHECK(tx1.find_write_set(x).success());
        CHECK(reinterpret_cast<int&>(tx1.write_set[0].pending_write()) == 44);
    
        CHECK(tx1.load(x) == 44);
        CHECK(tx0.load(x) == 43);
        CHECK(x.unsafe() == 42);
    
        tx1.store(x, 45);
        CHECK(tx1.write_set.size() == 1u);
        CHECK(tx1.find_write_set(x).success());
        CHECK(reinterpret_cast<int&>(tx1.write_set[0].pending_write()) == 45);
    
        CHECK(tx1.load(x) == 45);
        CHECK(tx0.load(x) == 43);
        CHECK(x.unsafe() == 42);
    
        tx0.cleanup();
        tx1.cleanup();
    }
    return test_result();
}
