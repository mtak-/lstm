#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>

#include <lstm/thread_data.hpp>

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

        thread_data* tls_td;
        gp_t         version_;

        inline transaction(thread_data& in_tls_td, const gp_t in_version) noexcept
            : tls_td(&in_tls_td)
            , version_(in_version)
        {
            assert(!tls_td->in_transaction());
            assert(version_ != detail::off_state);
            assert(!detail::locked(version_));
            assert(valid());
        }

        inline void reset_version(const gp_t new_version) noexcept
        {
            assert(tls_td->tx == this);
            assert(version_ <= new_version);
            version_ = new_version;

            assert(version_ != detail::off_state);
            assert(!detail::locked(version_));
            assert(valid());
        }

        detail::var_storage read_impl(const detail::var_base& src_var) const
        {
            assert(valid());

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

        // atomic var's perform no allocation (therefore, no callbacks)
        void atomic_write_impl(detail::var_base& dest_var, const detail::var_storage storage) const
        {
            assert(valid());

            const detail::write_set_lookup lookup = tls_td->write_set.lookup(dest_var);
            if (LSTM_LIKELY(!lookup.success()))
                tls_td->add_write_set(dest_var, storage, lookup.hash());
            else
                lookup.pending_write() = storage;

            if (!rw_valid(dest_var))
                detail::internal_retry();
        }

    public:
        thread_data& get_thread_data() const noexcept { return *tls_td; }
        gp_t         version() const noexcept { return version_; }

        bool valid(const thread_data& td = tls_thread_data()) const noexcept
        {
            return &td == tls_td && tls_td->active.load(LSTM_RELAXED) == version_;
        }

        bool rw_valid(const gp_t version) const noexcept { return version <= version_; }
        bool rw_valid(const detail::var_base& v) const noexcept
        {
            return rw_valid(v.version_lock.load(LSTM_RELAXED));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& read(const var<T, Alloc>& src_var) const
        {
            static_assert(std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(read_impl(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T read(const var<T, Alloc>& src_var) const
        {
            static_assert(!std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(read_impl(src_var));
        }

        // TODO: tease out the parts of this function that don't depend on template params
        template<typename T,
                 typename Alloc,
                 typename U = T,
                 LSTM_REQUIRES_(!var<T, Alloc>::atomic && std::is_assignable<T&, U&&>()
                                && std::is_constructible<T, U&&>())>
        void write(var<T, Alloc>& dest_var, U&& u) const
        {
            assert(valid());

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
        LSTM_ALWAYS_INLINE void write(var<T, Alloc>& dest_var, U&& u) const
        {
            atomic_write_impl(dest_var, dest_var.allocate_construct((U &&) u));
        }

        // reading/writing an rvalue probably never makes sense
        template<typename T, typename Alloc>
        void read(var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void read(const var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void write(var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void write(const var<T, Alloc>&& v) const = delete;
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */
