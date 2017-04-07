#ifndef LSTM_THREAD_DATA_HPP
#define LSTM_THREAD_DATA_HPP

#include <lstm/detail/pod_hash_set.hpp>
#include <lstm/detail/quiescence_buffer.hpp>
#include <lstm/detail/read_set_value_type.hpp>
#include <lstm/detail/thread_synchronization.hpp>
#include <lstm/detail/var_detail.hpp>
#include <lstm/detail/write_set_value_type.hpp>

#ifdef LSTM_USE_BOOST_FIBERS
#include <boost/fiber/fss.hpp>
#endif

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
            detail::quiescence_buffer<> d;
            tx_kind                     e;
            void*                       desired;
        };
        static constexpr std::size_t synchronization_cache_line_offset
            = offsetof(_cache_line_offset_calculation, desired) % LSTM_CACHE_LINE_SIZE;

        read_set_t                                                             read_set;
        write_set_t                                                            write_set;
        callbacks_t                                                            fail_callbacks;
        detail::quiescence_buffer<>                                            succ_callbacks;
        tx_kind                                                                tx_state;
        detail::thread_synchronization_node<synchronization_cache_line_offset> synchronization_node;

        void add_write_set_unchecked(detail::var_base&         dest_var,
                                     const detail::var_storage pending_write,
                                     const detail::hash_t      hash) noexcept
        {
            LSTM_ASSERT(!write_set.allocates_on_next_push());
            LSTM_ASSERT(in_critical_section());
            LSTM_ASSERT(in_read_write_transaction());

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

            write_set.push_back(&dest_var, pending_write, hash);
        }

        void clear_read_write_sets() noexcept
        {
            read_set.clear();
            write_set.clear();
        }

        void do_succ_callbacks_front() noexcept
        {
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(!in_critical_section());

            succ_callbacks.do_first_epoch_callbacks();
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
            LSTM_ASSERT(!succ_callbacks.empty());

            synchronize_min_epoch(succ_callbacks.back_epoch());
            do {
                do_succ_callbacks_front();
            } while (!succ_callbacks.empty());
        }

        LSTM_ALWAYS_INLINE void reclaim_all_possible(const epoch_t min_epoch) noexcept
        {
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(!in_critical_section());
            LSTM_ASSERT(!succ_callbacks.empty());

            do {
                do_succ_callbacks_front();
            } while (!succ_callbacks.empty() && succ_callbacks.front_epoch() < min_epoch);
        }

        LSTM_NOINLINE_LUKEWARM void reclaim_slow_path() noexcept
        {
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(!in_critical_section());

            const epoch_t min_epoch = synchronize_min_epoch(succ_callbacks.front_epoch());
            reclaim_all_possible(min_epoch);
        }

    public:
        LSTM_NOINLINE thread_data() noexcept
            : tx_state(tx_kind::none)
        {
            LSTM_ASSERT(std::uintptr_t(this) % LSTM_CACHE_LINE_SIZE == 0);
        }

        thread_data(const thread_data&) = delete;
        thread_data& operator=(const thread_data&) = delete;

        LSTM_NOINLINE ~thread_data() noexcept
        {
            LSTM_ASSERT(!in_critical_section());
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(read_set.empty());
            LSTM_ASSERT(write_set.empty());
            LSTM_ASSERT(fail_callbacks.empty());
            LSTM_ASSERT(succ_callbacks.working_epoch_empty());

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
            return synchronization_node.in_critical_section();
        }

        LSTM_ALWAYS_INLINE tx_kind tx_kind() const noexcept { return tx_state; }

        LSTM_ALWAYS_INLINE epoch_t epoch() const noexcept { return synchronization_node.epoch(); }

        LSTM_ALWAYS_INLINE void access_lock(const epoch_t epoch) noexcept
        {
            synchronization_node.access_lock(epoch);
        }

        LSTM_ALWAYS_INLINE void access_relock(const epoch_t epoch) noexcept
        {
            synchronization_node.access_relock(epoch);
        }

        LSTM_ALWAYS_INLINE void access_unlock() noexcept { synchronization_node.access_unlock(); }

        LSTM_ALWAYS_INLINE bool epoch_less_equal_to(const epoch_t epoch) const noexcept
        {
            return synchronization_node.epoch_less_equal_to(epoch);
        }

        LSTM_ALWAYS_INLINE epoch_t synchronize_min_epoch(const epoch_t sync_epoch) const noexcept
        {
            LSTM_ASSERT(!in_transaction());
            return synchronization_node.synchronize_min_epoch(sync_epoch);
        }

        template<typename Func,
                 LSTM_REQUIRES_(std::is_constructible<detail::gp_callback, Func&&>{})>
        void sometime_synchronized_after(Func&& func) noexcept(
            noexcept(succ_callbacks.emplace_back((Func &&) func)))
        {
            LSTM_ASSERT(in_transaction());
            succ_callbacks.emplace_back((Func &&) func);
        }

        template<typename Func,
                 LSTM_REQUIRES_(std::is_constructible<detail::gp_callback, Func&&>{})>
        void after_fail(Func&& func) noexcept(noexcept(fail_callbacks.emplace_back((Func &&) func)))
        {
            LSTM_ASSERT(in_transaction());
            fail_callbacks.emplace_back((Func &&) func);
        }

        void reclaim(const epoch_t sync_epoch) noexcept
        {
            LSTM_ASSERT(!in_critical_section());
            LSTM_ASSERT(!in_transaction());
            LSTM_ASSERT(sync_epoch != detail::off_state);
            LSTM_ASSERT(!detail::locked(sync_epoch));

            if (LSTM_UNLIKELY(succ_callbacks.finalize_epoch(sync_epoch)))
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

#ifndef LSTM_USE_BOOST_FIBERS
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
#else
LSTM_DETAIL_BEGIN
    LSTM_NOINLINE inline void tls_data_init(
        boost::fibers::fiber_specific_ptr<thread_data> & tls_td_ptr) noexcept
    {
        LSTM_ASSERT(tls_td_ptr.get() == nullptr);
        tls_td_ptr.reset(::new thread_data());
    }
LSTM_DETAIL_END

LSTM_BEGIN
    LSTM_ALWAYS_INLINE thread_data& tls_thread_data() noexcept
    {
        static boost::fibers::fiber_specific_ptr<thread_data> tls_thread_data_ptr;
        if (tls_thread_data_ptr.get() == nullptr)
            detail::tls_data_init(tls_thread_data_ptr);
        return *tls_thread_data_ptr;
    }
LSTM_END
#endif /* LSTM_USE_BOOST_FIBERS */

#endif /* LSTM_THREAD_DATA_HPP */
