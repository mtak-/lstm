// Range v3 library
//
//  Copyright Eric Niebler 2014
//  Copyright Casey Carter 2015
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef LSTM_TEST_SIMPLE_TEST_HPP
#define LSTM_TEST_SIMPLE_TEST_HPP

#include <cstdlib>
#include <utility>
#include <iostream>
#include <thread>
#include <typeinfo>

namespace test_impl
{
    inline int &test_failures()
    {
        static int test_failures = 0;
        return test_failures;
    }

    template<typename T>
    struct streamable_base
    {};

    template<typename T>
    std::ostream &operator<<(std::ostream &sout, streamable_base<T> const &)
    {
        return sout << "<non-streamable type>";
    }

    template<typename T>
    struct streamable : streamable_base<T>
    {
    private:
        T const &t_;
    public:
        explicit streamable(T const &t) : t_(t) {}
        template<typename U = T>
        friend auto operator<<(std::ostream &sout, streamable const &s) ->
            decltype(sout << std::declval<std::remove_reference_t<U> const &>())
        {
            return sout << s.t_;
        }
    };

    template<typename T>
    streamable<T> stream(T const &t)
    {
        return streamable<T>{t};
    }

    template<typename T>
    struct R
    {
    private:
        char const *filename_;
        char const *expr_;
        char const *func_;
        T t_;
        int lineno_;
        bool dismissed_ = false;

        template<typename U>
        void oops(U const &u) const
        {
            std::cerr
                << "> ERROR: CHECK failed \"" << expr_ << "\"\n"
                << "> \t" << filename_ << '(' << lineno_ << ')' << '\n'
                << "> \t in function \"" << func_ << "\"\n";
            if(dismissed_)
                std::cerr
                    << "> \tEXPECTED: " << stream(u) << "\n> \tACTUAL: " << stream(t_) << '\n';
            ++test_failures();
        }
        void dismiss()
        {
            dismissed_ = true;
        }
        template<typename V = T>
        auto eval_(int) -> decltype(!std::declval<V>())
        {
            return !t_;
        }
        bool eval_(long)
        {
            return true;
        }
    public:
        R(char const *filename, int lineno, char const *expr, const char* func, T && t)
          : filename_(filename), expr_(expr), func_(func)
          , t_(std::forward<T>(t)), lineno_(lineno)
        {}
        ~R()
        {
            if(!dismissed_ && eval_(42))
                this->oops(42);
        }
        template<typename U>
        void operator==(U const &u)
        {
            dismiss();
            if(!(t_ == u)) this->oops(u);
        }
        template<typename U>
        void operator!=(U const &u)
        {
            dismiss();
            if(!(t_ != u)) this->oops(u);
        }
        template<typename U>
        void operator<(U const &u)
        {
            dismiss();
            if(!(t_ < u)) this->oops(u);
        }
        template<typename U>
        void operator<=(U const &u)
        {
            dismiss();
            if(!(t_ <= u)) this->oops(u);
        }
        template<typename U>
        void operator>(U const &u)
        {
            dismiss();
            if(!(t_ > u)) this->oops(u);
        }
        template<typename U>
        void operator>=(U const &u)
        {
            dismiss();
            if(!(t_ >= u)) this->oops(u);
        }
    };

    struct S
    {
    private:
        char const *filename_;
        char const *expr_;
        char const *func_;
        int lineno_;
    public:
        S(char const *filename, int lineno, char const *expr, char const *func)
          : filename_(filename), expr_(expr), func_(func), lineno_(lineno)
        {}
        template<typename T>
        R<T> operator->*(T && t)
        {
            return {filename_, lineno_, expr_, func_, std::forward<T>(t)};
        }
    };
}

inline int test_result()
{
    return ::test_impl::test_failures() ? EXIT_FAILURE : EXIT_SUCCESS;
}

template<typename...>
using void_ = void;

template<template<typename...> class T, typename Arg, typename = void>
struct valid_instantiation : std::false_type {};

template<template<typename...> class T, typename Arg>
struct valid_instantiation<T, Arg, void_<T<Arg>>> : std::true_type {};

template<typename T, typename U>
constexpr auto Same = std::is_same<T, U>::value;

inline void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

#define CHECK(...)                                                                                 \
    (void)(::test_impl::S{__FILE__, __LINE__, #__VA_ARGS__, __PRETTY_FUNCTION__} ->* __VA_ARGS__)  \
    /**/
    
#define EXCEPT_CHECK(...)                                                                          \
    CHECK(__VA_ARGS__);                                                                            \
    CHECK(!noexcept(__VA_ARGS__))                                                                  \
    /**/

#define NOEXCEPT_CHECK(...)                                                                        \
    CHECK(__VA_ARGS__);                                                                            \
    CHECK(noexcept(__VA_ARGS__))                                                                   \
    /**/

#if defined(__has_feature)
    #if __has_feature(thread_sanitizer)
        #define LSTM_TEST_INIT(val, tsan_val) tsan_val
        #ifdef LSTM_LOG_TRANSACTIONS
            #error "LSTM_LOG_TRANSACTIONS will interfere with tsan results"
        #endif
        #ifndef NDEBUG
            #error "Not defining NDEBUG will interfere with tsan results"
        #endif
    #endif
#endif

#ifndef LSTM_TEST_INIT
    #define LSTM_TEST_INIT(val, tsan_val) val
#endif

#endif
