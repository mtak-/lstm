#ifndef LSTM_DETAIL_HTM_HPP
#define LSTM_DETAIL_HTM_HPP

#include <lstm/detail/lstm_fwd.hpp>

LSTM_DETAIL_BEGIN
    namespace htm
    {
#if defined(LSTM_HTM_ON) && defined(__x86_64__) && defined(__GNUC__)
        enum begin_code : unsigned int
        {
            started        = _XBEGIN_STARTED,
            abort_explicit = _XABORT_EXPLICIT,
            abort_retry    = _XABORT_RETRY,
            abort_conflict = _XABORT_CONFLICT,
            abort_capacity = _XABORT_CAPACITY,
            abort_debug    = _XABORT_DEBUG,
            abort_nested   = _XABORT_NESTED,
        };
        inline unsigned int get_abort_code(const begin_code c) noexcept { return _XABORT_CODE(c); }
#else
        enum begin_code : unsigned int
        {
            started,
            abort_explicit,
            abort_retry,
            abort_conflict,
            abort_capacity,
            abort_debug,
            abort_nested,
        };
        inline unsigned int get_abort_code(const begin_code c) noexcept { return (c >> 24) & 0xFF; }
#endif

#if defined(LSTM_HTM_ON) && defined(__x86_64__) && defined(__GNUC__)
        inline begin_code begin() noexcept { return static_cast<begin_code>(_xbegin()); }
#else
        // assumes capacity will always lead into the STM
        inline constexpr begin_code begin() noexcept { return abort_capacity; }
#endif

        inline void end() noexcept
        {
#if defined(LSTM_HTM_ON) && defined(__x86_64__) && defined(__GNUC__)
            _xend();
#else
            std::terminate(); // oops, bug, shouldn't call end()
#endif
        }

        inline bool in_hardware_tx() noexcept
        {
#if defined(LSTM_HTM_ON) && defined(__x86_64__) && defined(__GNUC__)
            return _xtest();
#else
            return false;
#endif
        }

        [[noreturn]] inline void abort() noexcept
        {
#if defined(LSTM_HTM_ON) && defined(__x86_64__) && defined(__GNUC__)
            _xabort(1);
            __builtin_unreachable();
#else
            std::terminate(); // oops, bug, shouldn't call abort()
#endif
        }

#ifdef LSTM_HTM_ON
#if defined(__x86_64__) && defined(__GNUC__)
        inline bool _cas_release(uint64_t* ptr, uint64_t oldVal, uint64_t newVal)
        {
            unsigned char ret;
            __asm__ __volatile__("  xrelease\n"
                                 "  lock\n"
                                 "  cmpxchgq %2,%1\n"
                                 "  sete %0\n"
                                 : "=q"(ret), "=m"(*ptr)
                                 : "r"(newVal), "m"(*ptr), "a"(oldVal)
                                 : "memory");
            return static_cast<bool>(ret);
        }

        inline bool _cas_release(uint32_t* ptr, uint32_t oldVal, uint32_t newVal)
        {
            unsigned char ret;
            __asm__ __volatile__("  xrelease\n"
                                 "  lock\n"
                                 "  cmpxchgl %2,%1\n"
                                 "  sete %0\n"
                                 : "=q"(ret), "=m"(*ptr)
                                 : "r"(newVal), "m"(*ptr), "a"(oldVal)
                                 : "memory");
            return static_cast<bool>(ret);
        }

        inline bool _cas_acquire(unsigned long* ptr, uint64_t oldVal, uint64_t newVal)
        {
            unsigned char ret;
            __asm__ __volatile__("  xacquire\n"
                                 "  lock\n"
                                 "  cmpxchgq %2,%1\n"
                                 "  sete %0\n"
                                 : "=q"(ret), "=m"(*ptr)
                                 : "r"(newVal), "m"(*ptr), "a"(oldVal)
                                 : "memory");
            return !static_cast<bool>(ret);
        }

        inline bool _cas_acquire(unsigned int* ptr, uint32_t oldVal, uint32_t newVal)
        {
            unsigned char ret;
            __asm__ __volatile__("  xacquire\n"
                                 "  lock\n"
                                 "  cmpxchgl %2,%1\n"
                                 "  sete %0\n"
                                 : "=q"(ret), "=m"(*ptr)
                                 : "r"(newVal), "m"(*ptr), "a"(oldVal)
                                 : "memory");
            return !static_cast<bool>(ret);
        }
#endif /* defined(__x86_64__) && defined(__GNUC__) */
#endif /* LSTM_HTM_ON */

        template<typename Integral>
        inline bool cmpxchg_strong_acquire(std::atomic<Integral>& v,
                                           Integral&              e,
                                           Integral               d,
                                           std::memory_order      m,
                                           std::memory_order      n) noexcept
        {
#if defined(LSTM_HTM_ON) && defined(__x86_64__) && defined(__GNUC__)
            (void)m;
            (void)n;
            if (sizeof(std::atomic<Integral>) == sizeof(Integral))
                return _cas_acquire(reinterpret_cast<Integral*>(&v), e, d);
#endif
            return v.compare_exchange_strong(e, d, m, n);
        }

        template<typename Integral>
        inline bool cmpxchg_strong_release(std::atomic<Integral>& v,
                                           Integral&              e,
                                           Integral               d,
                                           std::memory_order      m,
                                           std::memory_order      n) noexcept
        {
#if defined(LSTM_HTM_ON) && defined(__x86_64__) && defined(__GNUC__)
            (void)m;
            (void)n;
            if (sizeof(std::atomic<Integral>) == sizeof(Integral))
                return _cas_release(reinterpret_cast<Integral*>(&v), e, d);
#endif
            return v.compare_exchange_strong(e, d, m, n);
        }

        template<typename Integral>
        inline bool cmpxchg_weak_acquire(std::atomic<Integral>& v,
                                         Integral&              e,
                                         Integral               d,
                                         std::memory_order      m,
                                         std::memory_order      n) noexcept
        {
#if defined(LSTM_HTM_ON) && defined(__x86_64__) && defined(__GNUC__)
            (void)m;
            (void)n;
            if (sizeof(std::atomic<Integral>) == sizeof(Integral))
                return _cas_acquire(reinterpret_cast<Integral*>(&v), e, d);
#endif
            return v.compare_exchange_weak(e, d, m, n);
        }

        template<typename Integral>
        inline bool cmpxchg_weak_release(std::atomic<Integral>& v,
                                         Integral&              e,
                                         Integral               d,
                                         std::memory_order      m,
                                         std::memory_order      n) noexcept
        {
#if defined(LSTM_HTM_ON) && defined(__x86_64__) && defined(__GNUC__)
            (void)m;
            (void)n;
            if (sizeof(std::atomic<Integral>) == sizeof(Integral))
                return _cas_release(reinterpret_cast<Integral*>(&v), e, d);
#endif
            return v.compare_exchange_weak(e, d, m, n);
        }
    }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_HTM_HPP */