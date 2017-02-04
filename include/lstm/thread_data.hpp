#ifndef LSTM_THREAD_DATA_HPP
#define LSTM_THREAD_DATA_HPP

#include <lstm/detail/backoff.hpp>
#include <lstm/detail/gp_callback.hpp>
#include <lstm/detail/pod_hash_set.hpp>
#include <lstm/detail/pod_vector.hpp>
#include <lstm/detail/read_set_value_type.hpp>
#include <lstm/detail/write_set_value_type.hpp>

LSTM_DETAIL_BEGIN
    namespace
    {
        static constexpr gp_t off_state = ~gp_t(0);
    }

    struct global_data
    {
        mutex_type   thread_data_mut{};
        thread_data* thread_data_root{nullptr};
    };

    LSTM_INLINE_VAR LSTM_CACHE_ALIGNED global_data globals{};

    LSTM_NOINLINE inline thread_data& tls_data_init() noexcept;
LSTM_DETAIL_END

LSTM_BEGIN
    LSTM_ALWAYS_INLINE thread_data& tls_thread_data() noexcept;

    struct LSTM_CACHE_ALIGNED thread_data
    {
    private:
        friend struct detail::read_write_fn;
        friend transaction;
        friend LSTM_NOINLINE inline thread_data& detail::tls_data_init() noexcept;

        using read_set_t  = detail::pod_vector<detail::read_set_value_type>;
        using write_set_t = detail::pod_hash_set<detail::pod_vector<detail::write_set_value_type>>;
        using callbacks_t = detail::pod_vector<detail::gp_callback>;
        using read_set_const_iter = typename read_set_t::const_iterator;
        using write_set_iter      = typename write_set_t::iterator;
        using callbacks_iter      = typename callbacks_t::iterator;

        LSTM_CACHE_ALIGNED mutex_type mut;
        LSTM_CACHE_ALIGNED thread_data* next;
        transaction*                    tx;

        // TODO: this is not the best type for these callbacks as it doesn't support sharing
        // how could sharing be made fast for small tx's?
        callbacks_t        succ_callbacks;
        callbacks_t        fail_callbacks;
        LSTM_CACHE_ALIGNED std::atomic<gp_t> active;

        read_set_t  read_set;
        write_set_t write_set;

        static void lock_all() noexcept
        {
            LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_mut.lock();

            thread_data* current = LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
            while (current) {
                current->mut.lock();
                current = current->next;
            }
        }

        static void unlock_all() noexcept
        {
            thread_data* current = LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
            while (current) {
                current->mut.unlock();
                current = current->next;
            }

            LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_mut.unlock();
        }

        // TODO: allow specifying a backoff strategy
        static void wait(const gp_t gp) noexcept
        {
            for (thread_data* q = LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
                 q != nullptr;
                 q = q->next) {
                detail::default_backoff backoff;
                while (q->not_in_grace_period(gp))
                    backoff();
            }
        }

        void add_read_set(const detail::var_base& src_var) { read_set.emplace_back(&src_var); }

        void remove_read_set(const detail::var_base& src_var) noexcept
        {
            for (auto read_iter = read_set.begin(); read_iter < read_set.end(); ++read_iter) {
                while (read_iter < read_set.end() && read_iter->is_src_var(src_var))
                    read_set.unordered_erase(read_iter);
            }
        }

        void add_write_set(detail::var_base&         dest_var,
                           const detail::var_storage pending_write,
                           const detail::hash_t      hash)
        {
            // up to caller to ensure dest_var is not already in the write_set
            assert(!write_set.lookup(dest_var).success());
            remove_read_set(dest_var);
            write_set.push_back(&dest_var, pending_write, hash);
        }

        LSTM_NOINLINE thread_data() noexcept
            : tx(nullptr)
        {
            active.store(detail::off_state, LSTM_RELEASE);

            mut.lock();
            lock_all(); // this->mut does not get locked here
            {
                next = LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
                LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root = this;
            }
            unlock_all(); // this->mut gets unlocked here
        }

        LSTM_NOINLINE ~thread_data() noexcept
        {
            assert(tx == nullptr);
            assert(active.load(LSTM_RELAXED) == detail::off_state);

            lock_all(); // this->mut gets locked here
            {
                thread_data** indirect = &LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
                assert(*indirect);
                while (*indirect != this) {
                    indirect = &(*indirect)->next;
                    assert(*indirect);
                }
                *indirect = next;
            }
            unlock_all(); // this->mut does not get unlocked here
            mut.unlock();

            assert(succ_callbacks.empty());
            assert(fail_callbacks.empty());
            assert(read_set.empty());
            assert(write_set.empty());

            succ_callbacks.reset();
            fail_callbacks.reset();
            read_set.reset();
            write_set.reset();
        }

        thread_data(const thread_data&) = delete;
        thread_data& operator=(const thread_data&) = delete;

    public:
        inline bool in_transaction() const noexcept { return tx != nullptr; }
        inline bool in_critical_section() const noexcept
        {
            return active.load(LSTM_RELAXED) != detail::off_state;
        }

        inline void access_lock(const gp_t gp) noexcept
        {
            assert(tx == nullptr);
            assert(active.load(LSTM_RELAXED) == detail::off_state);
            assert(gp != detail::off_state);

            active.store(gp, LSTM_RELEASE);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }

        inline void access_relock(const gp_t gp) noexcept
        {
            assert(active.load(LSTM_RELAXED) != detail::off_state);
            assert(gp != detail::off_state);

            active.store(gp, LSTM_RELEASE);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }

        inline void access_unlock() noexcept
        {
            assert(active.load(LSTM_RELAXED) != detail::off_state);
            active.store(detail::off_state, LSTM_RELEASE);
        }

        inline bool not_in_grace_period(const gp_t gp) const noexcept
        {
            // TODO: acquire seems unneeded
            return active.load(LSTM_ACQUIRE) <= gp;
        }

        // TODO: allow specifying a backoff strategy
        inline void synchronize(const gp_t gp) noexcept
        {
            assert(tls_thread_data().active.load(LSTM_RELAXED) == detail::off_state);
            assert(&tls_thread_data().mut == &mut);
            assert(gp != detail::off_state);

            mut.lock();
            {
                wait(gp);
            }
            mut.unlock();
        }

        template<typename Func,
                 LSTM_REQUIRES_(std::is_constructible<detail::gp_callback, Func&&>{})>
        void
        queue_succ_callback(Func&& func) noexcept(noexcept(succ_callbacks.emplace_back((Func &&)
                                                                                           func)))
        {
            succ_callbacks.emplace_back((Func &&) func);
        }

        template<typename Func,
                 LSTM_REQUIRES_(std::is_constructible<detail::gp_callback, Func&&>{})>
        void
        queue_fail_callback(Func&& func) noexcept(noexcept(fail_callbacks.emplace_back((Func &&)
                                                                                           func)))
        {
            fail_callbacks.emplace_back((Func &&) func);
        }

        // TODO: when atomic swap on succ_callbacks is possible, this needs to do just that
        void do_succ_callbacks() noexcept
        {
#ifndef NDEBUG
            const std::size_t fail_start_size = fail_callbacks.size();
            const std::size_t succ_start_size = succ_callbacks.size();
#endif
            // TODO: if a callback adds a callback, this fails, again need a different type
            for (auto& succ_callback : succ_callbacks)
                succ_callback();

            assert(fail_start_size == fail_callbacks.size());
            assert(succ_start_size == succ_callbacks.size());

            succ_callbacks.clear();
        }

        // TODO: when atomic swap on succ_callbacks is possible, this needs to do just that
        void do_fail_callbacks() noexcept
        {
#ifndef NDEBUG
            const std::size_t fail_start_size = fail_callbacks.size();
            const std::size_t succ_start_size = succ_callbacks.size();
#endif
            // TODO: if a callback adds a callback, this fails, again need a different type
            const callbacks_iter begin = fail_callbacks.begin();
            for (callbacks_iter riter = fail_callbacks.end(); riter != begin;)
                (*--riter)();

            assert(fail_start_size == fail_callbacks.size());
            assert(succ_start_size == succ_callbacks.size());

            fail_callbacks.clear();
        }
    };

    namespace detail
    {
        LSTM_INLINE_VAR LSTM_THREAD_LOCAL thread_data* tls_thread_data_ptr = nullptr;

        // TODO: still feel like this garbage is overkill, maybe this only applies to darwin
        LSTM_NOINLINE inline thread_data& tls_data_init() noexcept
        {
            static LSTM_THREAD_LOCAL thread_data tls_thread_data{};
            LSTM_ACCESS_INLINE_VAR(tls_thread_data_ptr) = &tls_thread_data;
            return *LSTM_ACCESS_INLINE_VAR(tls_thread_data_ptr);
        }
    }

    LSTM_ALWAYS_INLINE thread_data& tls_thread_data() noexcept
    {
        if (LSTM_UNLIKELY(!LSTM_ACCESS_INLINE_VAR(detail::tls_thread_data_ptr)))
            return detail::tls_data_init();
        return *LSTM_ACCESS_INLINE_VAR(detail::tls_thread_data_ptr);
    }

    static_assert(std::is_standard_layout<thread_data>{}, "");
LSTM_END

#endif /* LSTM_THREAD_DATA_HPP */
