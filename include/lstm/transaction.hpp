#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>

#include <lstm/thread_data.hpp>
#include <lstm/transaction_domain.hpp>

#include <atomic>
#include <cassert>

LSTM_DETAIL_BEGIN
    [[noreturn]] LSTM_ALWAYS_INLINE void internal_retry() {
        LSTM_INTERNAL_FAIL_TX();
        throw detail::_tx_retry{};
    }
LSTM_DETAIL_END

LSTM_BEGIN
    [[noreturn]] inline void retry() {
        LSTM_USER_FAIL_TX();
        throw detail::_tx_retry{};
    }
    
    struct transaction {
    private:
        friend detail::read_write_fn;
        friend test::transaction_tester;
        
        using write_set_iter = typename thread_data::write_set_iter;
        
        static constexpr gp_t lock_bit = gp_t(1) << (sizeof(gp_t) * 8 - 1);
        
        transaction_domain* _domain;
        thread_data* tls_td;
        gp_t read_version;
        
        inline transaction_domain& domain() noexcept { return *_domain; }
        inline const transaction_domain& domain() const noexcept { return *_domain; }
        
        inline void reset_read_version() noexcept
        { tls_td->access_relock(read_version = domain().get_clock()); }
        
        inline transaction(transaction_domain& in_domain, thread_data& in_tls_td) noexcept
            : _domain(&in_domain)
            , tls_td(&in_tls_td)
        {
            assert(&tls_thread_data() == tls_td);
            assert(tls_td->tx == nullptr);
            assert(tls_td->active.load(LSTM_RELAXED) == detail::off_state);
            
            tls_td->access_lock(read_version = domain().get_clock());
        }
        
    #ifndef NDEBUG
        inline ~transaction() noexcept { assert(tls_td->tx == nullptr); }
    #endif
        
        static inline bool locked(const gp_t version) noexcept { return version & lock_bit; }
        static inline gp_t as_locked(const gp_t version) noexcept { return version | lock_bit; }
        
        inline bool rw_valid(const gp_t version) const noexcept
        { return version <= read_version; }
        
        inline bool rw_valid(const detail::var_base& v) const noexcept
        { return rw_valid(v.version_lock.load(LSTM_RELAXED)); }
        
        bool lock(detail::var_base& v) const noexcept {
            gp_t version_buf = v.version_lock.load(LSTM_RELAXED);
            return rw_valid(version_buf) &&
                    v.version_lock.compare_exchange_strong(version_buf,
                                                           as_locked(version_buf),
                                                           LSTM_ACQUIRE,
                                                           LSTM_RELAXED);
        }
        
        static inline void unlock(const gp_t version_to_set, detail::var_base& v) noexcept {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            assert(!locked(version_to_set));
            v.version_lock.store(version_to_set, LSTM_RELEASE);
        }
        
        static inline void unlock(detail::var_base& v) noexcept {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            v.version_lock.fetch_xor(lock_bit, LSTM_RELEASE);
        }
        
        static void unlock_write_set(write_set_iter begin, write_set_iter end) noexcept {
            for (; begin != end; ++begin)
                unlock(begin->dest_var());
        }
        
        // // this is not needed in practice but it does guarantee progress
        // void commit_sort_writes() noexcept
        // { std::sort(std::begin(tls_td->write_set), std::end(tls_td->write_set)); }
        
        bool commit_lock_writes() noexcept {
            write_set_iter write_begin = std::begin(tls_td->write_set);
            const write_set_iter write_end = std::end(tls_td->write_set);
            
            for (write_set_iter write_iter = write_begin; write_iter != write_end; ++write_iter) {
                // TODO: only care what version the var is, if it's also a read?
                if (!lock(write_iter->dest_var())) {
                    unlock_write_set(std::move(write_begin), std::move(write_iter));
                    return false;
                }
            }
            
            return true;
        }
        
        bool commit_validate_reads() noexcept {
            // reads do not need to be locked to be validated
            for (auto& read_set_vaue : tls_td->read_set) {
                if (!rw_valid(read_set_vaue.src_var())) {
                    unlock_write_set(std::begin(tls_td->write_set), std::end(tls_td->write_set));
                    return false;
                }
            }
            return true;
        }
        
        // TODO: emplace_back can throw exceptions...
        void commit_publish(const gp_t write_version) noexcept {
            for (auto& write_set_value : tls_td->write_set) {
                write_set_value.dest_var().storage.store(std::move(write_set_value.pending_write()),
                                                         LSTM_RELAXED);
                unlock(write_version, write_set_value.dest_var());
            }
        }
        
        void commit_reclaim_slow_path() noexcept {
            tls_td->synchronize(read_version);
            tls_td->do_succ_callbacks();
        }
        
        void commit_reclaim() noexcept {
            // TODO: batching
            if (!tls_td->succ_callbacks.empty())
                commit_reclaim_slow_path();
        }
        
        bool commit_slow_path() noexcept {
            // commit_sort_writes(); // not needed in practice... but does guarantee progress
            
            if (!commit_lock_writes())
                return false;
            
            gp_t prev_write_version = domain().fetch_and_bump_clock();
            
            if (prev_write_version != read_version && !commit_validate_reads())
                return false;
            
            commit_publish(prev_write_version + 1);
            
            read_version = prev_write_version;
            
            return true;
        }
        
        // TODO: optimize for the following case?
        // write_set.size() == 1
        bool commit() noexcept {
            if (!tls_td->write_set.empty() && !commit_slow_path()) {
                LSTM_INTERNAL_FAIL_TX();
                return false;
            }
            
            LSTM_SUCC_TX();
            
            return true;
        }
        
        // TODO: rename this or consider moving it out of this class
        void cleanup() noexcept {
            tls_td->write_set.clear();
            tls_td->read_set.clear();
            tls_td->succ_callbacks.clear();
            tls_td->do_fail_callbacks();
        }
        
        // TODO: rename this or consider moving it out of this class
        void reset_heap() noexcept {
            tls_td->write_set.clear();
            tls_td->read_set.clear();
            tls_td->fail_callbacks.clear();
        }
        
        detail::var_storage read_impl(const detail::var_base& src_var) {
            const detail::write_set_lookup lookup = tls_td->find_write_set(src_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const gp_t src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                const detail::var_storage result = src_var.storage.load(LSTM_ACQUIRE);
                if (rw_valid(src_version)
                        && src_var.version_lock.load(LSTM_RELAXED) == src_version) {
                    tls_td->add_read_set(src_var);
                    return result;
                }
            } else if (rw_valid(src_var))
                return lookup.pending_write();
            detail::internal_retry();
        }
        
        void atomic_write_impl(detail::var_base& dest_var, const detail::var_storage storage) {
            const detail::write_set_lookup lookup = tls_td->find_write_set(dest_var);
            if (LSTM_LIKELY(!lookup.success()))
                tls_td->add_write_set(dest_var, storage, lookup.hash());
            else
                lookup.pending_write() = storage;
            
            if (!rw_valid(dest_var)) detail::internal_retry();
        }
        
    public:
        // transactions can only be passed by non-const lvalue reference
        transaction(const transaction&) = delete;
        transaction(transaction&&) = delete;
        transaction& operator=(const transaction&) = delete;
        transaction& operator=(transaction&&) = delete;
        
        template<typename T, typename Alloc0,
            LSTM_REQUIRES_(!var<T, Alloc0>::atomic)>
        LSTM_ALWAYS_INLINE const T& read(const var<T, Alloc0>& src_var) {
            static_assert(std::is_reference<decltype(var<T>::load(src_var.storage.load()))>{}, "");
            return var<T, Alloc0>::load(read_impl(src_var));
        }
        
        template<typename T, typename Alloc0,
            LSTM_REQUIRES_(var<T, Alloc0>::atomic)>
        LSTM_ALWAYS_INLINE T read(const var<T, Alloc0>& src_var) {
            static_assert(!std::is_reference<decltype(var<T>::load(src_var.storage.load()))>{}, "");
            return var<T, Alloc0>::load(read_impl(src_var));
        }
        
        // TODO: tease out the parts of this function that don't depend on template params
        // probly don't wanna call allocate_construct unless it will for sure be used
        template<typename T, typename Alloc0, typename U = T,
            LSTM_REQUIRES_(!var<T, Alloc0>::atomic &&
                           std::is_assignable<T&, U&&>() &&
                           std::is_constructible<T, U&&>())>
        void write(var<T, Alloc0>& dest_var, U&& u) {
            const detail::write_set_lookup lookup = tls_td->find_write_set(dest_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const gp_t dest_version = dest_var.version_lock.load(LSTM_ACQUIRE);
                const detail::var_storage cur_storage = dest_var.storage.load(LSTM_ACQUIRE);
                if (rw_valid(dest_version)
                        && dest_var.version_lock.load(LSTM_RELAXED) == dest_version) {
                    const detail::var_storage new_storage = dest_var.allocate_construct((U&&)u);
                    tls_td->add_write_set(dest_var, new_storage, lookup.hash());
                    tls_td->queue_succ_callback([alloc = dest_var.alloc(),
                                                 cur_storage]() mutable noexcept {
                        var<T, Alloc0>::destroy_deallocate(alloc, cur_storage);
                    });
                    tls_td->queue_fail_callback([alloc = dest_var.alloc(),
                                                 new_storage]() mutable noexcept {
                        var<T, Alloc0>::destroy_deallocate(alloc, new_storage);
                    });
                    return;
                }
            }
            else if (rw_valid(dest_var)) {
                var<T>::store(lookup.pending_write(), (U&&)u);
                return;
            }
            
            detail::internal_retry();
        }
        
        template<typename T, typename Alloc0, typename U = T,
            LSTM_REQUIRES_(var<T, Alloc0>::atomic &&
                           std::is_assignable<T&, U&&>() &&
                           std::is_constructible<T, U&&>())>
        LSTM_ALWAYS_INLINE void write(var<T, Alloc0>& dest_var, U&& u)
        { atomic_write_impl(dest_var, dest_var.allocate_construct((U&&)u)); }
        
        // reading/writing an rvalue probably never makes sense
        template<typename T, typename Alloc0> void read(var<T, Alloc0>&& v) = delete;
        template<typename T, typename Alloc0> void read(const var<T, Alloc0>&& v) = delete;
        template<typename T, typename Alloc0> void write(var<T, Alloc0>&& v) = delete;
        template<typename T, typename Alloc0> void write(const var<T, Alloc0>&& v) = delete;
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */
