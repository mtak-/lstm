#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>
#include <lstm/detail/pod_hash_set.hpp>

#include <lstm/thread_data.hpp>
#include <lstm/transaction_domain.hpp>

#include <atomic>
#include <cassert>

LSTM_DETAIL_BEGIN
    template<typename T, typename Alloc>
    struct deleter {
    private:
        T* ptr;
        Alloc* alloc;
        
        using alloc_traits = std::allocator_traits<Alloc>;
        
    public:
        deleter() noexcept = default;
        
        deleter(T* in_ptr, Alloc* in_alloc) noexcept
            : ptr(in_ptr)
            , alloc(in_alloc)
        { assert(ptr && alloc); }
        
        void operator()() const noexcept {
            alloc_traits::destroy(*alloc, ptr);
            alloc_traits::deallocate(*alloc, ptr, 1);
        }
    };
    
    template<typename T>
    struct deleter<T, void> {
    private:
        T* ptr;
        
    public:
        deleter() noexcept = default;
        
        deleter(T* in_ptr, void*) noexcept
            : ptr(in_ptr)
        { assert(ptr); }
        
        void operator()() const noexcept { delete ptr; }
    };
    
    struct write_set_lookup {
        var_storage* pending_write_;
        hash_t hash;
        
        inline constexpr bool success() const noexcept { return !hash; }
        inline constexpr var_storage& pending_write() const noexcept { return *pending_write_; }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    [[noreturn]] inline void retry() {
        LSTM_USER_FAIL_TX();
        throw detail::_tx_retry{};
    }
    
    struct transaction {
    protected:
        friend detail::read_write_fn;
        friend test::transaction_tester;
        
        using read_set_const_iter = typename thread_data::read_set_t::const_iterator;
        using write_set_iter = typename thread_data::write_set_t::iterator;
        
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
        
        bool lock(const detail::var_base& v) const noexcept {
            gp_t version_buf = v.version_lock.load(LSTM_RELAXED);
            // TODO: not convinced of this ordering
            return rw_valid(version_buf) &&
                    v.version_lock.compare_exchange_strong(version_buf,
                                                           as_locked(version_buf),
                                                           LSTM_RELEASE);
        }
        
        static inline void unlock(const gp_t version_to_set, const detail::var_base& v) noexcept {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            assert(!locked(version_to_set));
            v.version_lock.store(version_to_set, LSTM_RELEASE);
        }
        
        static inline void unlock(const detail::var_base& v) noexcept {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            v.version_lock.fetch_xor(lock_bit, LSTM_RELEASE);
        }
        
        void add_read_set(const detail::var_base& src_var)
        { tls_td->read_set.emplace_back(&src_var); }
        
        void add_write_set(detail::var_base& dest_var,
                           const detail::var_storage pending_write,
                           const detail::hash_t hash) {
            // up to caller to ensure dest_var is not already in the write_set
            assert(!find_write_set(dest_var).success());
            tls_td->write_set.push_back(&dest_var, pending_write, hash);
        }
        
        detail::write_set_lookup find_write_set(const detail::var_base& dest_var) noexcept {
            detail::write_set_lookup lookup;
            lookup.hash = hash(dest_var);
            if (LSTM_LIKELY(!(tls_td->write_set.filter() & lookup.hash)))
                return lookup;
            write_set_iter iter = slow_find(tls_td->write_set.begin(), tls_td->write_set.end(), dest_var);
            return iter != tls_td->write_set.end()
                ? detail::write_set_lookup{&iter->pending_write(), 0}
                : lookup;
        }
        
        static void unlock_write_set(write_set_iter begin, write_set_iter end) noexcept {
            for (; begin != end; ++begin)
                unlock(begin->dest_var());
        }
        
        void commit_sort_writes() noexcept
        { std::sort(std::begin(tls_td->write_set), std::end(tls_td->write_set)); }
        
        void commit_remove_writes_from_reads() noexcept {
            // using the bloom filter seemed to be mostly unhelpful
            for (auto& write_elem : tls_td->write_set) {
                // TODO: weird to have this here
                // typical usage patterns would probly be:
                //   read shared variable
                //   do stuff
                //   store result
                //   end transaction
                // transactions are pretty composable which might matter a little
                tls_td->read_set.set_end(std::remove_if(
                            std::begin(tls_td->read_set),
                            std::end(tls_td->read_set),
                            [&](const detail::read_set_value_type& rsv) noexcept {
                                return rsv.is_src_var(write_elem.dest_var());
                            }));
            }
        }
        
        bool commit_lock_writes() noexcept {
            // TODO: substantial performance hit from these two calls :(
            commit_sort_writes(); // not needed in practice... but does guarantee progress
            commit_remove_writes_from_reads();
            
            write_set_iter write_begin = std::begin(tls_td->write_set);
            write_set_iter write_end = std::end(tls_td->write_set);
            
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
            if (!commit_lock_writes())
                return false;
            
            gp_t write_version = domain().bump_clock();
            
            if (write_version != read_version && !commit_validate_reads())
                return false;
            
            commit_publish(write_version + 1);
            
            read_version = write_version;
            
            return true;
        }
        
        // TODO: optimize for the following case?
        // write_set.size() == 1 && (read_set.empty() ||
        //                           read_set.size() == 1 && read_set[0] == write_set[0])
        bool commit() noexcept {
            if ((!tls_td->write_set.empty() || !tls_td->succ_callbacks.empty()) && !commit_slow_path()) {
                LSTM_INTERNAL_FAIL_TX();
                return false;
            }
            
            LSTM_SUCC_TX();
            
            return true;
        }
        
        // if the transaction failed, cleanup calls a virtual function (:o) on var's
        // therefore, access_lock() must be active
        void cleanup() noexcept {
            // TODO: destroy only???
            tls_td->write_set.clear();
            tls_td->read_set.clear();
            
            tls_td->do_fail_callbacks();
            
            // TODO: batching
            tls_td->succ_callbacks.clear();
        }
        
        void reset_heap() noexcept {
            tls_td->write_set.clear();
            tls_td->read_set.clear();
            tls_td->fail_callbacks.clear();
        }
        
    public:
        // transactions can only be passed by non-const lvalue reference
        transaction(const transaction&) = delete;
        transaction(transaction&&) = delete;
        transaction& operator=(const transaction&) = delete;
        transaction& operator=(transaction&&) = delete;
        
        template<typename T, typename Alloc0,
            LSTM_REQUIRES_(!var<T, Alloc0>::atomic)>
        const T& read(const var<T, Alloc0>& src_var) {
            static_assert(std::is_reference<decltype(var<T>::load(src_var.storage.load()))>{}, "");
                      
            detail::write_set_lookup lookup = find_write_set(src_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const gp_t src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                const T& result = var<T>::load(src_var.storage.load(LSTM_ACQUIRE));
                if (rw_valid(src_version)
                        && src_var.version_lock.load(LSTM_RELAXED) == src_version) {
                    add_read_set(src_var);
                    return result;
                }
            } else if (rw_valid(src_var))
                return var<T>::load(lookup.pending_write());
            detail::internal_retry();
        }
        
        template<typename T, typename Alloc0,
            LSTM_REQUIRES_(var<T, Alloc0>::atomic)>
        T read(const var<T, Alloc0>& src_var) {
            static_assert(!std::is_reference<decltype(var<T>::load(src_var.storage.load()))>{}, "");
            
            detail::write_set_lookup lookup = find_write_set(src_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const gp_t src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                const T result = var<T>::load(src_var.storage.load(LSTM_ACQUIRE));
                if (rw_valid(src_version)
                        && src_var.version_lock.load(LSTM_RELAXED) == src_version) {
                    add_read_set(src_var);
                    return result;
                }
            } else if (rw_valid(src_var))
                return var<T>::load(lookup.pending_write());
            detail::internal_retry();
        }
        
        template<typename T, typename Alloc0, typename U,
            LSTM_REQUIRES_(!var<T, Alloc0>::atomic &&
                           std::is_assignable<T&, U&&>() &&
                           std::is_constructible<T, U&&>())>
        void write(var<T, Alloc0>& dest_var, U&& u) {
            detail::write_set_lookup lookup = find_write_set(dest_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const gp_t dest_version = dest_var.version_lock.load(LSTM_ACQUIRE);
                const detail::var_storage cur_storage = dest_var.storage.load(LSTM_ACQUIRE);
                if (rw_valid(dest_version)
                        && dest_var.version_lock.load(LSTM_RELAXED) == dest_version) {
                    const detail::var_storage new_storage = dest_var.allocate_construct((U&&)u);
                    add_write_set(dest_var, new_storage, lookup.hash);
                    tls_td->succ_callbacks.emplace_back([dest_var = &dest_var,
                                                         cur_storage] {
                        dest_var->destroy_deallocate(cur_storage);
                    });
                    tls_td->fail_callbacks.emplace_back([dest_var = &dest_var,
                                                         new_storage] {
                        dest_var->destroy_deallocate(new_storage);
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
        
        template<typename T, typename Alloc0, typename U,
            LSTM_REQUIRES_(var<T, Alloc0>::atomic &&
                           std::is_assignable<T&, U&&>() &&
                           std::is_constructible<T, U&&>())>
        void write(var<T, Alloc0>& dest_var, U&& u) {
            detail::write_set_lookup lookup = find_write_set(dest_var);
            if (LSTM_LIKELY(!lookup.success()))
                add_write_set(dest_var, dest_var.allocate_construct((U&&)u), lookup.hash);
            else
                var<T>::store(lookup.pending_write(), (U&&)u);
            
            if (!rw_valid(dest_var)) detail::internal_retry();
        }
        
        template<typename T, typename Alloc = void>
        void delete_(T* dest_var, Alloc* alloc = nullptr)
        { tls_td->succ_callbacks.emplace_back(detail::deleter<T, Alloc>(dest_var, alloc)); }
        
        // reading/writing an rvalue probably never makes sense
        template<typename T, typename Alloc0> void read(var<T, Alloc0>&& v) = delete;
        template<typename T, typename Alloc0> void read(const var<T, Alloc0>&& v) = delete;
        template<typename T, typename Alloc0> void write(var<T, Alloc0>&& v) = delete;
        template<typename T, typename Alloc0> void write(const var<T, Alloc0>&& v) = delete;
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */
