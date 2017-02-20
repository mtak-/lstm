#ifndef LSTM_THREAD_DATA_HPP
#define LSTM_THREAD_DATA_HPP

#include <lstm/detail/fast_rw_mutex.hpp>
#include <lstm/detail/gp_callback.hpp>
#include <lstm/detail/pod_hash_set.hpp>
#include <lstm/detail/read_set_value_type.hpp>
#include <lstm/detail/reclaim_buffer.hpp>
#include <lstm/detail/var_detail.hpp>
#include <lstm/detail/write_set_value_type.hpp>

LSTM_DETAIL_BEGIN
    using mutex_type = fast_rw_mutex;

    namespace
    {
        static_assert(std::is_unsigned<gp_t>{}, "");

        static constexpr gp_t lock_bit  = gp_t(1) << (sizeof(gp_t) * 8 - 1);
        static constexpr gp_t off_state = ~gp_t(0);
    }

    inline bool locked(const gp_t version) noexcept { return version & lock_bit; }
    inline gp_t as_locked(const gp_t version) noexcept { return version | lock_bit; }

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

    struct thread_data
    {
    private:
        friend detail::read_write_fn;
        friend detail::commit_algorithm;
        friend transaction;
        friend LSTM_NOINLINE inline thread_data& detail::tls_data_init() noexcept;

        using read_set_t  = detail::pod_vector<detail::read_set_value_type>;
        using write_set_t = detail::pod_hash_set<detail::pod_vector<detail::write_set_value_type>>;
        using callbacks_t = detail::pod_vector<detail::gp_callback>;
        using read_set_const_iter = typename read_set_t::const_iterator;
        using write_set_iter      = typename write_set_t::iterator;
        using callbacks_iter      = typename callbacks_t::iterator;

        // TODO: optimize this layout once batching is implemented
        LSTM_CACHE_ALIGNED detail::mutex_type mut;
        thread_data*                          next;
        std::atomic<gp_t>                     active;
        bool                                  in_transaction_;
        read_set_t                            read_set;
        write_set_t                           write_set;
        callbacks_t                           fail_callbacks;
        detail::succ_callbacks_t<4>           succ_callbacks;

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

        // TODO: allow specifying a backoff strategy
        static bool try_wait(const gp_t gp) noexcept
        {
            for (thread_data* q = LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
                 q != nullptr;
                 q = q->next) {
                if (q->not_in_grace_period(gp))
                    return false;
            }
            return true;
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

        void reclaim_all() noexcept
        {
            synchronize(succ_callbacks.back().version);
            do {
                do_succ_callbacks(succ_callbacks.front().callbacks);
                succ_callbacks.pop_front();
            } while (!succ_callbacks.empty());
        }

        LSTM_ALWAYS_INLINE void reclaim_all_possible() noexcept
        {
            do {
                do_succ_callbacks(succ_callbacks.front().callbacks);
                succ_callbacks.pop_front();
            } while (!succ_callbacks.empty() && try_wait(succ_callbacks.front().version));
        }

        LSTM_NOINLINE void reclaim_slow_path() noexcept
        {
            mut.lock_shared();
            {
                wait(succ_callbacks.front().version);
                reclaim_all_possible();
            }
            mut.unlock_shared();
        }

        LSTM_NOINLINE thread_data() noexcept
            : in_transaction_(false)
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
            assert(!in_transaction());
            assert(!in_critical_section());
            assert(read_set.empty());
            assert(write_set.empty());
            assert(fail_callbacks.empty());
            assert(succ_callbacks.active().callbacks.empty());

            if (!succ_callbacks.empty())
                reclaim_all();

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
        }

        thread_data(const thread_data&) = delete;
        thread_data& operator=(const thread_data&) = delete;

    public:
        inline bool in_transaction() const noexcept { return in_transaction_; }
        inline bool in_critical_section() const noexcept
        {
            return active.load(LSTM_RELAXED) != detail::off_state;
        }

        inline void access_lock(const gp_t gp) noexcept
        {
            assert(!in_transaction());
            assert(!in_critical_section());
            assert(gp != detail::off_state);

            active.store(gp, LSTM_RELEASE);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }

        inline void access_relock(const gp_t gp) noexcept
        {
            assert(in_critical_section());
            assert(gp != detail::off_state);
            assert(active.load(LSTM_RELAXED) <= gp);

            active.store(gp, LSTM_RELEASE);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }

        inline void access_unlock() noexcept
        {
            assert(in_critical_section());
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
            assert(!in_critical_section());
            assert(gp != detail::off_state);

            mut.lock_shared();
            {
                wait(gp);
            }
            mut.unlock_shared();
        }

        template<typename Func,
                 LSTM_REQUIRES_(std::is_constructible<detail::gp_callback, Func&&>{})>
        void queue_succ_callback(Func&& func) noexcept(
            noexcept(succ_callbacks.active().callbacks.emplace_back((Func &&) func)))
        {
            succ_callbacks.active().callbacks.emplace_back((Func &&) func);
        }

        template<typename Func,
                 LSTM_REQUIRES_(std::is_constructible<detail::gp_callback, Func&&>{})>
        void
        queue_fail_callback(Func&& func) noexcept(noexcept(fail_callbacks.emplace_back((Func &&)
                                                                                           func)))
        {
            fail_callbacks.emplace_back((Func &&) func);
        }

        static void do_succ_callbacks(callbacks_t& succ_callbacks) noexcept
        {
            for (detail::gp_callback& succ_callback : succ_callbacks)
                succ_callback();
            succ_callbacks.clear();
        }

        void do_fail_callbacks() noexcept
        {
#ifndef NDEBUG
            const std::size_t fail_start_size = fail_callbacks.size();
#endif
            const callbacks_iter begin = fail_callbacks.begin();
            for (callbacks_iter riter = fail_callbacks.end(); riter != begin;)
                (*--riter)();

            assert(fail_start_size == fail_callbacks.size());

            fail_callbacks.clear();
        }

        void reclaim(const gp_t sync_version) noexcept
        {
            assert(!in_critical_section());
            assert(sync_version != detail::off_state);
            assert(!detail::locked(sync_version));

            detail::succ_callback_t& active_buf = succ_callbacks.active();
            if (active_buf.callbacks.empty())
                return;

            if (LSTM_UNLIKELY(succ_callbacks.push_is_full(sync_version)))
                reclaim_slow_path();
        }

        // clears up all buffers that greedily hold on to extra storage
        void shrink_to_fit() noexcept(noexcept(read_set.shrink_to_fit(),
                                               write_set.shrink_to_fit(),
                                               fail_callbacks.shrink_to_fit(),
                                               succ_callbacks.shrink_to_fit()))
        {
            if (!in_critical_section() && !succ_callbacks.empty())
                reclaim_all();
            read_set.shrink_to_fit();
            write_set.shrink_to_fit();
            fail_callbacks.shrink_to_fit();
            succ_callbacks.shrink_to_fit();
        }
    };
LSTM_END

LSTM_DETAIL_BEGIN
    LSTM_INLINE_VAR LSTM_THREAD_LOCAL thread_data* tls_thread_data_ptr = nullptr;

    // TODO: still feel like this garbage is overkill, maybe this only applies to darwin
    LSTM_NOINLINE inline thread_data& tls_data_init() noexcept
    {
        static LSTM_THREAD_LOCAL thread_data tls_thread_data{};
        LSTM_ACCESS_INLINE_VAR(tls_thread_data_ptr) = &tls_thread_data;
        return *LSTM_ACCESS_INLINE_VAR(tls_thread_data_ptr);
    }
LSTM_DETAIL_END

LSTM_BEGIN
    LSTM_ALWAYS_INLINE thread_data& tls_thread_data() noexcept
    {
        if (LSTM_UNLIKELY(!LSTM_ACCESS_INLINE_VAR(detail::tls_thread_data_ptr)))
            return detail::tls_data_init();
        return *LSTM_ACCESS_INLINE_VAR(detail::tls_thread_data_ptr);
    }
LSTM_END

#endif /* LSTM_THREAD_DATA_HPP */
