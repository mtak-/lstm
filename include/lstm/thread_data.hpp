#ifndef LSTM_THREAD_DATA_HPP
#define LSTM_THREAD_DATA_HPP

#include <lstm/detail/gp_callback.hpp>
#include <lstm/detail/pod_hash_set.hpp>
#include <lstm/detail/read_set_value_type.hpp>
#include <lstm/detail/reclaim_buffer.hpp>
#include <lstm/detail/thread_gp.hpp>
#include <lstm/detail/var_detail.hpp>
#include <lstm/detail/write_set_value_type.hpp>

LSTM_BEGIN
    enum class tx_kind : char
    {
        none = 0,
        read_write,
        read_only
    };

    struct LSTM_CACHE_ALIGNED thread_data
    {
    private:
        friend detail::atomic_base_fn;
        friend detail::commit_algorithm;
        friend detail::transaction_base;

        using read_set_t  = detail::pod_vector<detail::read_set_value_type>;
        using write_set_t = detail::pod_hash_set<detail::pod_vector<detail::write_set_value_type>>;
        using callbacks_t = detail::pod_vector<detail::gp_callback>;
        using read_set_const_iter = typename read_set_t::const_iterator;
        using write_set_iter      = typename write_set_t::iterator;
        using callbacks_iter      = typename callbacks_t::iterator;

        // TODO: optimize this layout
        struct _cache_line_offset_calculation
        {
            read_set_t                  a;
            write_set_t                 b;
            callbacks_t                 c;
            detail::succ_callbacks_t<4> d;
            tx_kind                     e;
            int*                        desired;
        };
        static constexpr std::size_t tgp_cache_line_offset
            = offsetof(_cache_line_offset_calculation, desired) % LSTM_CACHE_LINE_SIZE;

        read_set_t                                    read_set;
        write_set_t                                   write_set;
        callbacks_t                                   fail_callbacks;
        detail::succ_callbacks_t<4>                   succ_callbacks;
        tx_kind                                       tx_state;
        detail::thread_gp_node<tgp_cache_line_offset> tgp_node;

        void remove_read_set(const detail::var_base& src_var) noexcept
        {
            LSTM_ASSERT(in_critical_section());
            LSTM_ASSERT(in_read_write_transaction());

            const read_set_const_iter begin = read_set.begin();
            for (read_set_const_iter read_iter = read_set.end(); read_iter != begin;) {
                --read_iter;
                if (read_iter->is_src_var(src_var))
                    read_set.unordered_erase(read_iter);
            }
        }

        void add_write_set_unchecked(detail::var_base&         dest_var,
                                     const detail::var_storage pending_write,
                                     const detail::hash_t      hash) noexcept
        {
            LSTM_ASSERT(!write_set.allocates_on_next_push());
            LSTM_ASSERT(in_critical_section());
            LSTM_ASSERT(in_read_write_transaction());

            remove_read_set(dest_var);
            write_set.unchecked_push_back(&dest_var, pending_write, hash);
        }

        void add_write_set(
            detail::var_base&         dest_var,
            const detail::var_storage pending_write,
            const detail::hash_t      hash) noexcept(noexcept(write_set.push_back(&dest_var,
                                                                             pending_write,
                                                                             hash)))
        {
            LSTM_ASSERT(in_critical_section());
            LSTM_ASSERT(in_read_write_transaction());

            remove_read_set(dest_var);
            write_set.push_back(&dest_var, pending_write, hash);
        }

        void clear_read_write_sets() noexcept
        {
            LSTM_LOG_READ_AND_WRITE_SET_SIZE(read_set.size(), write_set.size());
            read_set.clear();
            write_set.clear();
        }

        void do_succ_callbacks_front() noexcept
        {
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(!in_critical_section());

            callbacks_t& callbacks = succ_callbacks.front().callbacks;
            succ_callbacks.pop_front();
            for (detail::gp_callback& callback : callbacks)
                callback();
            callbacks.clear();
        }

        void do_fail_callbacks() noexcept
        {
#ifndef NDEBUG
            const std::size_t fail_start_size = fail_callbacks.size();
#endif
            const callbacks_iter begin = fail_callbacks.begin();
            for (callbacks_iter riter = fail_callbacks.end(); riter != begin;)
                (*--riter)();

#ifndef NDEBUG
            LSTM_ASSERT(fail_start_size == fail_callbacks.size());
#endif

            fail_callbacks.clear();
        }

        void reclaim_all() noexcept
        {
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(!in_critical_section());

            synchronize_min_gp(succ_callbacks.back().version);
            do {
                do_succ_callbacks_front();
            } while (!succ_callbacks.empty());
        }

        LSTM_ALWAYS_INLINE void reclaim_all_possible(const gp_t min_gp) noexcept
        {
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(!in_critical_section());

            do {
                do_succ_callbacks_front();
            } while (!succ_callbacks.empty() && succ_callbacks.front().version < min_gp);
        }

        LSTM_NOINLINE_LUKEWARM void reclaim_slow_path() noexcept
        {
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(!in_critical_section());

            const gp_t min_gp = synchronize_min_gp(succ_callbacks.front().version);
            reclaim_all_possible(min_gp);
        }

    public:
        LSTM_NOINLINE thread_data() noexcept
            : tx_state(tx_kind::none)
        {
        }

        thread_data(const thread_data&) = delete;
        thread_data& operator=(const thread_data&) = delete;

        LSTM_NOINLINE ~thread_data() noexcept
        {
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(read_set.empty());
            LSTM_ASSERT(write_set.empty());
            LSTM_ASSERT(fail_callbacks.empty());
            LSTM_ASSERT(succ_callbacks.active().callbacks.empty());

            if (!succ_callbacks.empty())
                reclaim_all();
        }

        LSTM_ALWAYS_INLINE bool in_transaction() const noexcept
        {
            return tx_state != tx_kind::none;
        }

        LSTM_ALWAYS_INLINE bool in_read_only_transaction() const noexcept
        {
            return tx_state == tx_kind::read_only;
        }

        LSTM_ALWAYS_INLINE bool in_read_write_transaction() const noexcept
        {
            return tx_state == tx_kind::read_write;
        }

        LSTM_ALWAYS_INLINE bool in_critical_section() const noexcept
        {
            return tgp_node.in_critical_section();
        }

        LSTM_ALWAYS_INLINE tx_kind tx_kind() const noexcept { return tx_state; }

        LSTM_ALWAYS_INLINE gp_t gp() const noexcept { return tgp_node.gp(); }

        LSTM_ALWAYS_INLINE void access_lock(const gp_t gp) noexcept
        {
            LSTM_ASSERT(!in_transaction());
            tgp_node.access_lock(gp);
        }

        LSTM_ALWAYS_INLINE void access_relock(const gp_t gp) noexcept
        {
            tgp_node.access_relock(gp);
        }

        LSTM_ALWAYS_INLINE void access_unlock() noexcept { tgp_node.access_unlock(); }

        LSTM_ALWAYS_INLINE bool not_in_grace_period(const gp_t gp) const noexcept
        {
            return tgp_node.not_in_grace_period(gp);
        }

        LSTM_ALWAYS_INLINE gp_t synchronize_min_gp(const gp_t sync_version) const noexcept
        {
            LSTM_ASSERT(!in_transaction());
            return tgp_node.synchronize_min_gp(sync_version);
        }

        template<typename Func,
                 LSTM_REQUIRES_(std::is_constructible<detail::gp_callback, Func&&>{})>
        void sometime_synchronized_after(Func&& func) noexcept(
            noexcept(succ_callbacks.active().callbacks.emplace_back((Func &&) func)))
        {
            LSTM_ASSERT(in_transaction());
            succ_callbacks.active().callbacks.emplace_back((Func &&) func);
        }

        template<typename Func,
                 LSTM_REQUIRES_(std::is_constructible<detail::gp_callback, Func&&>{})>
        void after_fail(Func&& func) noexcept(noexcept(fail_callbacks.emplace_back((Func &&) func)))
        {
            LSTM_ASSERT(in_transaction());
            fail_callbacks.emplace_back((Func &&) func);
        }

        void reclaim(const gp_t sync_version) noexcept
        {
            LSTM_ASSERT(!in_critical_section());
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(sync_version != detail::off_state);
            LSTM_ASSERT(!detail::locked(sync_version));

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
