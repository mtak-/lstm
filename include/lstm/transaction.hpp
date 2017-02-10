#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>

#include <lstm/thread_data.hpp>
#include <lstm/transaction_domain.hpp>

#include <atomic>
#include <cassert>

LSTM_DETAIL_BEGIN
    [[noreturn]] LSTM_ALWAYS_INLINE void internal_retry()
    {
        LSTM_INTERNAL_FAIL_TX();
        throw tx_retry{};
    }
LSTM_DETAIL_END

LSTM_BEGIN
    struct transaction
    {
    private:
        friend detail::read_write_fn;
        friend test::transaction_tester;

        using write_set_iter = typename thread_data::write_set_iter;

        static constexpr gp_t lock_bit = gp_t(1) << (sizeof(gp_t) * 8 - 1);

        thread_data* tls_td;
        gp_t         read_version_;

        inline void reset_read_version(const gp_t new_read_version) noexcept
        {
            assert(read_version_ <= new_read_version);
            read_version_ = new_read_version;
        }

        inline transaction(thread_data& in_tls_td, const gp_t in_read_version) noexcept
            : tls_td(&in_tls_td)
            , read_version_(in_read_version)
        {
            assert(&tls_thread_data() == tls_td);
            assert(tls_td->tx == nullptr);
            assert(tls_td->active.load(LSTM_RELAXED) != detail::off_state);
        }

        static inline bool locked(const gp_t version) noexcept { return version & lock_bit; }
        static inline gp_t as_locked(const gp_t version) noexcept { return version | lock_bit; }

        inline bool rw_valid(const gp_t version) const noexcept { return version <= read_version_; }
        inline bool rw_valid(const detail::var_base& v) const noexcept
        {
            return rw_valid(v.version_lock.load(LSTM_RELAXED));
        }

        bool lock(detail::var_base& v) const noexcept
        {
            gp_t version_buf = v.version_lock.load(LSTM_RELAXED);
            return rw_valid(version_buf)
                   && v.version_lock.compare_exchange_strong(version_buf,
                                                             as_locked(version_buf),
                                                             LSTM_ACQUIRE,
                                                             LSTM_RELAXED);
        }

        // x86_64: likely compiles to mov
        static inline void unlock(const gp_t version_to_set, detail::var_base& v) noexcept
        {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            assert(!locked(version_to_set));

            v.version_lock.store(version_to_set, LSTM_RELEASE);
        }

        // x86: likely compiles to xor
        static inline void unlock(detail::var_base& v) noexcept
        {
            const gp_t locked_version = v.version_lock.load(LSTM_RELAXED);

            assert(locked(locked_version));

            v.version_lock.store(locked_version ^ lock_bit, LSTM_RELEASE);
        }

        static void unlock_write_set(write_set_iter begin, const write_set_iter end) noexcept
        {
            for (; begin != end; ++begin)
                unlock(begin->dest_var());
        }

        bool commit_lock_writes() noexcept
        {
            write_set_iter       write_begin = std::begin(tls_td->write_set);
            const write_set_iter write_end   = std::end(tls_td->write_set);

            for (write_set_iter write_iter = write_begin; write_iter != write_end; ++write_iter) {
                if (!lock(write_iter->dest_var())) {
                    unlock_write_set(std::move(write_begin), std::move(write_iter));
                    return false;
                }
            }

            return true;
        }

        bool commit_validate_reads() noexcept
        {
            for (detail::read_set_value_type read_set_vaue : tls_td->read_set) {
                if (!rw_valid(read_set_vaue.src_var())) {
                    unlock_write_set(std::begin(tls_td->write_set), std::end(tls_td->write_set));
                    return false;
                }
            }
            return true;
        }

        void commit_publish(const gp_t write_version) noexcept
        {
            for (detail::write_set_value_type write_set_value : tls_td->write_set) {
                write_set_value.dest_var().storage.store(write_set_value.pending_write(),
                                                         LSTM_RELAXED);
                unlock(write_version, write_set_value.dest_var());
            }
        }

        void commit_reclaim_slow_path() noexcept
        {
            tls_td->synchronize(read_version_);
            tls_td->do_succ_callbacks();
        }

        void commit_reclaim() noexcept
        {
            // TODO: batching
            if (!tls_td->succ_callbacks.empty())
                commit_reclaim_slow_path();
        }

        void commit_slowerer_path(const gp_t prev_write_version) noexcept
        {
            commit_publish(prev_write_version + 1);

            read_version_ = prev_write_version;
        }

        bool commit_slower_path(transaction_domain& domain) noexcept
        {
            const gp_t prev_write_version = domain.fetch_and_bump_clock();

            if (prev_write_version != read_version_ && !commit_validate_reads())
                return false;
            commit_slowerer_path(prev_write_version);
            return true;
        }

        bool commit_slow_path(transaction_domain& domain) noexcept
        {
            if (!commit_lock_writes())
                return false;
            return commit_slower_path(domain);
        }

        bool commit(transaction_domain& domain) noexcept
        {
            if (!tls_td->write_set.empty() && !commit_slow_path(domain)) {
                LSTM_INTERNAL_FAIL_TX();
                return false;
            }

            LSTM_SUCC_TX();

            return true;
        }

        // TODO: rename this or consider moving it out of this class
        void cleanup() noexcept
        {
            tls_td->write_set.clear();
            tls_td->read_set.clear();
            tls_td->succ_callbacks.clear();
            tls_td->do_fail_callbacks();
        }

        // TODO: rename this or consider moving it out of this class
        void reset_heap() noexcept
        {
            tls_td->write_set.clear();
            tls_td->read_set.clear();
            tls_td->fail_callbacks.clear();
        }

        detail::var_storage read_impl(const detail::var_base& src_var)
        {
            const detail::write_set_lookup lookup = tls_td->write_set.lookup(src_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const gp_t                src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                const detail::var_storage result      = src_var.storage.load(LSTM_ACQUIRE);
                if (rw_valid(src_version)
                    && src_var.version_lock.load(LSTM_RELAXED) == src_version) {
                    tls_td->add_read_set(src_var);
                    return result;
                }
            } else if (rw_valid(src_var)) {
                return lookup.pending_write();
            }
            detail::internal_retry();
        }

        void atomic_write_impl(detail::var_base& dest_var, const detail::var_storage storage)
        {
            const detail::write_set_lookup lookup = tls_td->write_set.lookup(dest_var);
            if (LSTM_LIKELY(!lookup.success()))
                tls_td->add_write_set(dest_var, storage, lookup.hash());
            else
                lookup.pending_write() = storage;

            if (!rw_valid(dest_var))
                detail::internal_retry();
        }

    public:
        gp_t read_version() const noexcept { return read_version_; }

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& read(const var<T, Alloc>& src_var)
        {
            static_assert(std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(read_impl(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T read(const var<T, Alloc>& src_var)
        {
            static_assert(!std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(read_impl(src_var));
        }

        // TODO: tease out the parts of this function that don't depend on template params
        // probly don't wanna call allocate_construct unless it will for sure be used
        template<typename T,
                 typename Alloc,
                 typename U = T,
                 LSTM_REQUIRES_(!var<T, Alloc>::atomic && std::is_assignable<T&, U&&>()
                                && std::is_constructible<T, U&&>())>
        void write(var<T, Alloc>& dest_var, U&& u)
        {
            const detail::write_set_lookup lookup = tls_td->write_set.lookup(dest_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const gp_t                dest_version = dest_var.version_lock.load(LSTM_ACQUIRE);
                const detail::var_storage cur_storage  = dest_var.storage.load(LSTM_ACQUIRE);
                if (rw_valid(dest_version)
                    && dest_var.version_lock.load(LSTM_RELAXED) == dest_version) {
                    const detail::var_storage new_storage = dest_var.allocate_construct((U &&) u);
                    tls_td->add_write_set(dest_var, new_storage, lookup.hash());
                    tls_td->queue_succ_callback(
                        [ alloc = dest_var.alloc(), cur_storage ]() mutable noexcept {
                            var<T, Alloc>::destroy_deallocate(alloc, cur_storage);
                        });
                    tls_td->queue_fail_callback(
                        [ alloc = dest_var.alloc(), new_storage ]() mutable noexcept {
                            var<T, Alloc>::destroy_deallocate(alloc, new_storage);
                        });
                    return;
                }
            } else if (rw_valid(dest_var)) {
                var<T>::store(lookup.pending_write(), (U &&) u);
                return;
            }

            detail::internal_retry();
        }

        template<typename T,
                 typename Alloc,
                 typename U = T,
                 LSTM_REQUIRES_(var<T, Alloc>::atomic&& std::is_assignable<T&, U&&>()
                                && std::is_constructible<T, U&&>())>
        LSTM_ALWAYS_INLINE void write(var<T, Alloc>& dest_var, U&& u)
        {
            atomic_write_impl(dest_var, dest_var.allocate_construct((U &&) u));
        }

        // reading/writing an rvalue probably never makes sense
        template<typename T, typename Alloc>
        void read(var<T, Alloc>&& v) = delete;
        template<typename T, typename Alloc>
        void read(const var<T, Alloc>&& v) = delete;
        template<typename T, typename Alloc>
        void write(var<T, Alloc>&& v) = delete;
        template<typename T, typename Alloc>
        void write(const var<T, Alloc>&& v) = delete;
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */
