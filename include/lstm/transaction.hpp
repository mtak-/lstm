#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/thread_data.hpp>

LSTM_BEGIN
    struct transaction
    {
    private:
        using write_set_const_iter = thread_data::write_set_t::const_iterator;

        thread_data* tls_td;
        gp_t         version_;

    protected:
        struct read_only_t
        {
        };
        static constexpr read_only_t read_only{};

        transaction(read_only_t, const gp_t in_version) noexcept
            : tls_td(nullptr)
            , version_(in_version)
        {
        }

        thread_data* raw_tls_td_ptr() const noexcept { return tls_td; }

    private:
        LSTM_NOINLINE detail::var_storage read_impl_slow_path(const detail::var_base& src_var) const
        {
            const write_set_const_iter iter = tls_td->write_set.find(src_var);
            if (iter == tls_td->write_set.end()) {
                const detail::var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (rw_valid(src_var.version_lock.load(LSTM_ACQUIRE))) {
                    tls_td->read_set.emplace_back(&src_var);
                    return result;
                }
            } else if (rw_valid(src_var)) {
                return iter->pending_write();
            }

            detail::internal_retry();
        }

    protected:
        LSTM_NOINLINE_LUKEWARM detail::var_storage read_impl(const detail::var_base& src_var) const
        {
            assert(valid());

            if (LSTM_LIKELY(!tls_td->read_set.allocates_on_next_push()
                            && !(tls_td->write_set.filter() & dumb_reference_hash(src_var)))) {
                const detail::var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (LSTM_LIKELY(rw_valid(src_var.version_lock.load(LSTM_ACQUIRE)))) {
                    tls_td->read_set.unchecked_emplace_back(&src_var);
                    return result;
                }
            }
            return read_impl_slow_path(src_var);
        }

    private:
        LSTM_NOINLINE void
        atomic_write_slow_path(detail::var_base& dest_var, const detail::var_storage storage) const
        {
            const detail::write_set_lookup lookup = tls_td->write_set.lookup(dest_var);
            if (LSTM_LIKELY(!lookup.success()))
                tls_td->add_write_set(dest_var, storage, lookup.hash());
            else
                lookup.pending_write() = storage;

            if (!rw_valid(dest_var))
                detail::internal_retry();
        }

    protected:
        // atomic var's perform no allocation (therefore, no callbacks)
        LSTM_NOINLINE_LUKEWARM void
        atomic_write_impl(detail::var_base& dest_var, const detail::var_storage storage) const
        {
            assert(valid());

            const detail::hash_t hash = dumb_reference_hash(dest_var);

            if (LSTM_UNLIKELY(tls_td->write_set.allocates_on_next_push()
                              || (tls_td->write_set.filter() & hash)
                              || !rw_valid(dest_var)))
                atomic_write_slow_path(dest_var, storage);
            else
                tls_td->add_write_set_unchecked(dest_var, storage, hash);
        }

    private:
        LSTM_NOINLINE detail::var_storage
        untracked_read_impl_slow_path(const detail::var_base& src_var) const
        {
            const write_set_const_iter iter = tls_td->write_set.find(src_var);
            if (iter == tls_td->write_set.end()) {
                const detail::var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (rw_valid(src_var.version_lock.load(LSTM_ACQUIRE)))
                    return result;
            } else if (rw_valid(src_var)) {
                return iter->pending_write();
            }

            detail::internal_retry();
        }

    protected:
        LSTM_NOINLINE_LUKEWARM detail::var_storage
        untracked_read_impl(const detail::var_base& src_var) const
        {
            assert(valid());

            if (LSTM_LIKELY(!(tls_td->write_set.filter() & dumb_reference_hash(src_var)))) {
                const detail::var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (LSTM_LIKELY(rw_valid(src_var.version_lock.load(LSTM_ACQUIRE))))
                    return result;
            }
            return untracked_read_impl_slow_path(src_var);
        }

    public:
        inline transaction(thread_data& in_tls_td, const gp_t in_version) noexcept
            : tls_td(&in_tls_td)
            , version_(in_version)
        {
            assert(version_ != detail::off_state);
            assert(!detail::locked(version_));
            assert(valid());
        }

        thread_data& get_thread_data() const noexcept { return *tls_td; }
        gp_t         version() const noexcept { return version_; }

        void reset_version(const gp_t new_version) noexcept
        {
            assert(version_ <= new_version);

            version_ = new_version;

            assert(version_ != detail::off_state);
            assert(!detail::locked(version_));
            assert(valid());
        }

        bool valid(const thread_data& td = tls_thread_data()) const noexcept
        {
            return &td == tls_td && tls_td->gp() == version_ && tls_td->in_transaction();
        }

        bool rw_valid(const gp_t version) const noexcept { return version <= version_; }
        bool rw_valid(const detail::var_base& v) const noexcept
        {
            return rw_valid(v.version_lock.load(LSTM_RELAXED));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_NOINLINE const T& read(const var<T, Alloc>& src_var) const
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
        // TODO: optimize it
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
                const detail::var_storage cur_storage = dest_var.storage.load(LSTM_ACQUIRE);
                if (LSTM_LIKELY(rw_valid(dest_var.version_lock.load(LSTM_ACQUIRE)))) {
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

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& untracked_read(const var<T, Alloc>& src_var) const
        {
            static_assert(std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(untracked_read_impl(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T untracked_read(const var<T, Alloc>& src_var) const
        {
            static_assert(!std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(untracked_read_impl(src_var));
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
        template<typename T, typename Alloc>
        void untracked_read(var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void untracked_read(const var<T, Alloc>&& v) const = delete;
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */
