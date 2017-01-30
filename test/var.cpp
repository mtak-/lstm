#include <lstm/var.hpp>

#include "simple_test.hpp"

#include <scoped_allocator>
#include <vector>

using lstm::var;

int main()
{
    CHECK(std::atomic<int>{0}.is_lock_free());
    // value tests
    {
        var<int> x;
        x.unsafe_write(42);

        NOEXCEPT_CHECK(x.unsafe_read() == 42);
    }
    {
        var<int> x{42};
        NOEXCEPT_CHECK(x.unsafe_read() == 42);

        x.unsafe_write(43);
        // static_assert(Same<int&, decltype(x.unsafe_read())>, "");
        NOEXCEPT_CHECK(x.unsafe_read() == 43);

        // static_assert(Same<int&&, decltype(std::move(x).unsafe_read())>, "");
    }
    {
        const var<int> x{42};
        NOEXCEPT_CHECK(x.unsafe_read() == 42);

        // static_assert(Same<const int&, decltype(x.unsafe())>, "");
        // static_assert(Same<const int&&, decltype(std::move(x).unsafe())>, "");
    }
    // allocators
    {
        var<std::vector<int>, std::scoped_allocator_adaptor<std::allocator<std::vector<int>>>> x_p;
        var<std::vector<int>, std::scoped_allocator_adaptor<std::allocator<std::vector<int>>>> x{};
        auto foo = x.unsafe_read();
        foo.push_back(42);
        x.unsafe_write(std::move(foo));

        CHECK(x.unsafe_read().size() == 1u);
        CHECK(x.unsafe_read()[0] == 42);
    }
    {
        using alloc = std::scoped_allocator_adaptor<std::allocator<std::vector<int>>>;
        var<std::vector<int>, alloc> x{std::allocator_arg, alloc{}};
        auto foo = x.unsafe_read();
        foo.push_back(42);
        x.unsafe_write(std::move(foo));

        CHECK(x.unsafe_read().size() == 1u);
        CHECK(x.unsafe_read()[0] == 42);
    }
    {
        using alloc = std::scoped_allocator_adaptor<std::allocator<std::vector<int>>>;
        var<std::vector<int>, alloc> x{std::allocator_arg, alloc{}, std::vector<int>{0, 1, 2, 3}};
        auto foo = x.unsafe_read();
        foo.push_back(42);
        x.unsafe_write(std::move(foo));

        CHECK(x.unsafe_read().size() == 5u);
        CHECK(x.unsafe_read()[0] == 0);
        CHECK(x.unsafe_read()[1] == 1);
        CHECK(x.unsafe_read()[2] == 2);
        CHECK(x.unsafe_read()[3] == 3);
        CHECK(x.unsafe_read()[4] == 42);
    }

    // non const
    static_assert(std::is_default_constructible<lstm::var<int>>{}, "");
    static_assert(std::is_constructible<lstm::var<int>>{}, "");
    static_assert(std::is_constructible<lstm::var<int>, int&>{}, "");
    static_assert(std::is_constructible<lstm::var<int>, int&&>{}, "");
    static_assert(std::is_constructible<lstm::var<int>, const int&>{}, "");
    static_assert(std::is_constructible<lstm::var<int>, const int&&>{}, "");
    static_assert(std::is_nothrow_default_constructible<lstm::var<int>>{}, "");
    static_assert(std::is_nothrow_constructible<lstm::var<int>>{}, "");
    static_assert(std::is_nothrow_constructible<lstm::var<int>, int&>{}, "");
    static_assert(std::is_nothrow_constructible<lstm::var<int>, int&&>{}, "");
    static_assert(std::is_nothrow_constructible<lstm::var<int>, const int&>{}, "");
    static_assert(std::is_nothrow_constructible<lstm::var<int>, const int&&>{}, "");
    static_assert(!std::is_copy_constructible<lstm::var<int>>{}, "");
    static_assert(!std::is_move_constructible<lstm::var<int>>{}, "");
    static_assert(std::is_destructible<lstm::var<int>>{}, "");

    // const
    // static_assert(!std::is_default_constructible<lstm::var<const int>>{}, "");
    // static_assert(!std::is_constructible<lstm::var<const int>>{}, "");
    // static_assert(!std::is_nothrow_constructible<lstm::var<const int>>{}, "");
    // static_assert(std::is_constructible<lstm::var<const int>, int&>{}, "");
    // static_assert(std::is_constructible<lstm::var<const int>, int&&>{}, "");
    // static_assert(std::is_constructible<lstm::var<const int>, const int&>{}, "");
    // static_assert(std::is_constructible<lstm::var<const int>, const int&&>{}, "");
    // static_assert(std::is_nothrow_constructible<lstm::var<const int>, int&>{}, "");
    // static_assert(std::is_nothrow_constructible<lstm::var<const int>, int&&>{}, "");
    // static_assert(std::is_nothrow_constructible<lstm::var<const int>, const int&>{}, "");
    // static_assert(std::is_nothrow_constructible<lstm::var<const int>, const int&&>{}, "");
    // static_assert(!std::is_copy_constructible<lstm::var<const int>>{}, "");
    // static_assert(!std::is_move_constructible<lstm::var<const int>>{}, "");
    // static_assert(std::is_destructible<lstm::var<const int>>{}, "");

    // ref
    // static_assert(std::is_constructible<lstm::var<int&>, int&>{}, "");
    // static_assert(!std::is_constructible<lstm::var<int&>, const int&>{}, "");
    // static_assert(std::is_constructible<lstm::var<const int&>, int&>{}, "");
    // static_assert(std::is_constructible<lstm::var<const int&>, const int&>{}, "");
    // static_assert(!std::is_constructible<lstm::var<int&&>, int&&>{}, "");

    return test_result();
}
