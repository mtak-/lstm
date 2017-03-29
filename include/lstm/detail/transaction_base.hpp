#ifndef LSTM_DETAIL_TRANSACTION_BASE_HPP
#define LSTM_DETAIL_TRANSACTION_BASE_HPP

#include <lstm/thread_data.hpp>

LSTM_DETAIL_BEGIN
    [[noreturn]] LSTM_NOINLINE inline void internal_retry() { throw tx_retry{}; }

    struct transaction_base
    {
    private:
        using write_set_const_iter = thread_data::write_set_t::const_iterator;

        thread_data* tls_td;
        epoch_t      version_;

        /*************************/
        /* read write operations */
        /*************************/
        LSTM_NOINLINE var_storage rw_read_slow_path(const var_base& src_var) const
        {
            const write_set_const_iter iter = tls_td->write_set.find(src_var);
            if (iter == tls_td->write_set.end()) {
                const var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (rw_valid(src_var.version_lock.load(LSTM_ACQUIRE))) {
                    tls_td->read_set.emplace_back(&src_var);
                    return result;
                }
            } else if (rw_valid(src_var)) {
                return iter->pending_write();
            }

            internal_retry();
        }

        LSTM_NOINLINE_LUKEWARM var_storage rw_read_base(const var_base& src_var) const
        {
            LSTM_ASSERT(valid(tls_td));

            if (LSTM_LIKELY(!tls_td->read_set.allocates_on_next_push()
                            && !(tls_td->write_set.filter() & dumb_reference_hash(src_var)))) {
                const var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (LSTM_LIKELY(rw_valid(src_var.version_lock.load(LSTM_ACQUIRE)))) {
                    tls_td->read_set.unchecked_emplace_back(&src_var);
                    return result;
                }
            }
            return rw_read_slow_path(src_var);
        }

        template<typename T,
                 typename Alloc,
                 typename U = T,
                 LSTM_REQUIRES_(!var<T, Alloc>::atomic && std::is_assignable<T&, U&&>()
                                && std::is_constructible<T, U&&>())>
        LSTM_NOINLINE void rw_write_slow_path(var<T, Alloc>& dest_var, U&& u) const
        {
            const write_set_lookup lookup = tls_td->write_set.lookup(dest_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const var_storage cur_storage = dest_var.storage.load(LSTM_ACQUIRE);
                if (LSTM_LIKELY(rw_valid(dest_var.version_lock.load(LSTM_ACQUIRE)))) {
                    const var_storage new_storage = dest_var.allocate_construct((U &&) u);
                    tls_td->add_write_set(dest_var, new_storage, lookup.hash());
                    sometime_synchronized_after(
                        [ alloc = dest_var.alloc(), cur_storage ]() mutable noexcept {
                            var<T, Alloc>::destroy_deallocate(alloc, cur_storage);
                        });
                    after_fail([ alloc = dest_var.alloc(), new_storage ]() mutable noexcept {
                        var<T, Alloc>::destroy_deallocate(alloc, new_storage);
                    });
                    return;
                }
            } else if (rw_valid(dest_var)) {
                var<T>::store(lookup.pending_write(), (U &&) u);
                return;
            }

            internal_retry();
        }

        LSTM_NOINLINE void
        rw_atomic_write_slow_path(var_base& dest_var, const var_storage storage) const
        {
            const write_set_lookup lookup = tls_td->write_set.lookup(dest_var);
            if (LSTM_LIKELY(!lookup.success()))
                tls_td->add_write_set(dest_var, storage, lookup.hash());
            else
                lookup.pending_write() = storage;

            if (!rw_valid(dest_var))
                internal_retry();
        }

        // atomic var's perform no allocation (therefore, no callbacks)
        LSTM_NOINLINE_LUKEWARM void
        rw_atomic_write_base(var_base& dest_var, const var_storage storage) const
        {
            LSTM_ASSERT(valid(tls_td));

            const hash_t hash = dumb_reference_hash(dest_var);

            if (LSTM_UNLIKELY(tls_td->write_set.allocates_on_next_push()
                              || (tls_td->write_set.filter() & hash)
                              || !rw_valid(dest_var)))
                rw_atomic_write_slow_path(dest_var, storage);
            else
                tls_td->add_write_set_unchecked(dest_var, storage, hash);
        }

        LSTM_NOINLINE var_storage rw_untracked_read_slow_path(const var_base& src_var) const
        {
            const write_set_const_iter iter = tls_td->write_set.find(src_var);
            if (iter == tls_td->write_set.end()) {
                const var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (rw_valid(src_var.version_lock.load(LSTM_ACQUIRE)))
                    return result;
            } else if (rw_valid(src_var)) {
                return iter->pending_write();
            }

            internal_retry();
        }

        LSTM_NOINLINE_LUKEWARM var_storage rw_untracked_read_base(const var_base& src_var) const
        {
            LSTM_ASSERT(valid(tls_td));

            if (LSTM_LIKELY(!(tls_td->write_set.filter() & dumb_reference_hash(src_var)))) {
                const var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (LSTM_LIKELY(rw_valid(src_var.version_lock.load(LSTM_ACQUIRE))))
                    return result;
            }
            return rw_untracked_read_slow_path(src_var);
        }

        /************************/
        /* read only operations */
        /************************/
        LSTM_NOINLINE var_storage ro_read_slow_path(const var_base& src_var) const
        {
            if (can_write())
                return rw_read_base(src_var);
            else
                internal_retry();
        }

        LSTM_NOINLINE_LUKEWARM var_storage ro_read_base(const var_base& src_var) const
        {
            LSTM_ASSERT(valid(tls_td));

            if (LSTM_LIKELY(!can_write())) {
                const var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (LSTM_LIKELY(rw_valid(src_var.version_lock.load(LSTM_ACQUIRE))))
                    return result;
            }
            return ro_read_slow_path(src_var);
        }

        LSTM_NOINLINE var_storage ro_untracked_read_slow_path(const var_base& src_var) const
        {
            if (can_write())
                return rw_untracked_read_base(src_var);
            else
                internal_retry();
        }

        LSTM_NOINLINE_LUKEWARM var_storage ro_untracked_read_base(const var_base& src_var) const
        {
            LSTM_ASSERT(valid(tls_td));

            if (LSTM_LIKELY(!can_write())) {
                const var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (LSTM_LIKELY(rw_valid(src_var.version_lock.load(LSTM_ACQUIRE))))
                    return result;
            }
            return ro_untracked_read_slow_path(src_var);
        }

    public:
        inline transaction_base(thread_data* const in_tls_td, const epoch_t in_version) noexcept
            : tls_td(in_tls_td)
            , version_(in_version)
        {
            LSTM_ASSERT(version_ != off_state);
            LSTM_ASSERT(!locked(version_));
            LSTM_ASSERT(valid(tls_td));
        }

        thread_data& get_thread_data() const noexcept { return *tls_td; }
        epoch_t      version() const noexcept { return version_; }

        void unsafe_reset_version(const epoch_t new_version) noexcept
        {
            // TODO: could these asserts fail for correct code?
            if (tls_td) {
                LSTM_ASSERT(tls_td->write_set.empty());
                LSTM_ASSERT(tls_td->read_set.empty());
                LSTM_ASSERT(tls_td->fail_callbacks.empty());
                LSTM_ASSERT(tls_td->succ_callbacks.working_epoch_empty());
            }

            LSTM_ASSERT(version_ <= new_version);

            version_ = new_version;

            LSTM_ASSERT(version_ != off_state);
            LSTM_ASSERT(!locked(version_));
            LSTM_ASSERT(valid(tls_td));
        }

        bool can_write() const noexcept { return tls_td; }
        bool can_demote_safely() const noexcept { return tls_td->write_set.empty(); }

        bool valid(const thread_data* td) const noexcept
        {
            if (td)
                return ((!tls_td && td->tx_state != tx_kind::read_write)
                        || ((td == tls_td && td->tx_state != tx_kind::read_only)
                            || td->write_set.empty()))
                       && td->epoch() == version_ && td->in_transaction();
            else
                return tls_td == nullptr;
        }

        bool rw_valid(const epoch_t version) const noexcept { return version <= version_; }
        bool rw_valid(const var_base& v) const noexcept
        {
            return rw_valid(v.version_lock.load(LSTM_RELAXED));
        }

        /*************************/
        /* read write operations */
        /*************************/
        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& rw_read(const var<T, Alloc>& src_var) const
        {
            static_assert(std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(rw_read_base(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T rw_read(const var<T, Alloc>& src_var) const
        {
            static_assert(!std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(rw_read_base(src_var));
        }

        // TODO: tease out the parts of this function that don't depend on template params
        // TODO: optimize it
        template<typename T,
                 typename Alloc,
                 typename U = T,
                 LSTM_REQUIRES_(!var<T, Alloc>::atomic && std::is_assignable<T&, U&&>()
                                && std::is_constructible<T, U&&>())>
        LSTM_NOINLINE_LUKEWARM void rw_write(var<T, Alloc>& dest_var, U&& u) const
        {
            LSTM_ASSERT(valid(tls_td));

            const hash_t hash = dumb_reference_hash(dest_var);

            if (LSTM_UNLIKELY(tls_td->write_set.allocates_on_next_push()
                              || (tls_td->write_set.filter() & hash)
                              || tls_td->fail_callbacks.allocates_on_next_push()
                              || tls_td->succ_callbacks.allocates_on_next_push()
                              || !rw_valid(dest_var))) {
                rw_write_slow_path(dest_var, (U &&) u);
            } else {
                const var_storage new_storage = dest_var.allocate_construct((U &&) u);
                tls_td->add_write_set_unchecked(dest_var, new_storage, hash);
                const var_storage cur_storage = dest_var.storage.load(LSTM_RELAXED);
                tls_td->fail_callbacks.unchecked_emplace_back(
                    [ alloc = dest_var.alloc(), new_storage ]() mutable noexcept {
                        var<T, Alloc>::destroy_deallocate(alloc, new_storage);
                    });
                tls_td->succ_callbacks.unchecked_emplace_back(
                    [ alloc = dest_var.alloc(), cur_storage ]() mutable noexcept {
                        var<T, Alloc>::destroy_deallocate(alloc, cur_storage);
                    });
            }
        }

        template<typename T,
                 typename Alloc,
                 typename U = T,
                 LSTM_REQUIRES_(var<T, Alloc>::atomic&& std::is_assignable<T&, U&&>()
                                && std::is_constructible<T, U&&>())>
        LSTM_ALWAYS_INLINE void rw_write(var<T, Alloc>& dest_var, U&& u) const
        {
            rw_atomic_write_base(dest_var, dest_var.allocate_construct((U &&) u));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& rw_untracked_read(const var<T, Alloc>& src_var) const
        {
            static_assert(std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(rw_untracked_read_base(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T rw_untracked_read(const var<T, Alloc>& src_var) const
        {
            static_assert(!std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(rw_untracked_read_base(src_var));
        }

        /************************/
        /* read only operations */
        /************************/
        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& ro_read(const var<T, Alloc>& src_var) const
        {
            static_assert(std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(ro_read_base(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T ro_read(const var<T, Alloc>& src_var) const
        {
            static_assert(!std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(ro_read_base(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& ro_untracked_read(const var<T, Alloc>& src_var) const
        {
            static_assert(std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(ro_untracked_read_base(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T ro_untracked_read(const var<T, Alloc>& src_var) const
        {
            static_assert(!std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(ro_untracked_read_base(src_var));
        }

        template<typename Func, LSTM_REQUIRES_(std::is_constructible<gp_callback, Func&&>{})>
        void sometime_synchronized_after(Func&& func) const
            noexcept(noexcept(tls_td->sometime_synchronized_after((Func &&) func)))
        {
            LSTM_ASSERT(tls_td);
            LSTM_ASSERT(valid(tls_td));
            tls_td->sometime_synchronized_after((Func &&) func);
        }

        template<typename Func, LSTM_REQUIRES_(std::is_constructible<gp_callback, Func&&>{})>
        void after_fail(Func&& func) const noexcept(noexcept(tls_td->after_fail((Func &&) func)))
        {
            LSTM_ASSERT(tls_td);
            LSTM_ASSERT(valid(tls_td));
            tls_td->after_fail((Func &&) func);
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_TRANSACTION_BASE_HPP */