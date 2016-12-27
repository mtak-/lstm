#ifndef LSTM_DETAIL_TRANSACTION_IMPL_HPP
#define LSTM_DETAIL_TRANSACTION_IMPL_HPP

#include <lstm/detail/read_set_value_type.hpp>
#include <lstm/detail/write_set_deleter.hpp>
#include <lstm/detail/write_set_value_type.hpp>

#include <lstm/transaction.hpp>

#include <algorithm>

LSTM_DETAIL_BEGIN
    template<typename Alloc, std::size_t ReadSize, std::size_t WriteSize, std::size_t DeleteSize>
    struct transaction_impl : lstm::transaction {
        friend detail::read_write_fn;
        friend test::transaction_tester;
    private:
        using alloc_traits = std::allocator_traits<Alloc>;
        using read_alloc = typename alloc_traits::template rebind_alloc<read_set_value_type>;
        using write_alloc = typename alloc_traits::template rebind_alloc<write_set_value_type>;
        using read_set_t = small_pod_vector<read_set_value_type, ReadSize, read_alloc>;
        using write_set_t = pod_hash_set<small_pod_vector<write_set_value_type,
                                                          WriteSize,
                                                          write_alloc>>;
        using read_set_const_iter = typename read_set_t::const_iterator;
        using write_set_iter = typename write_set_t::iterator;
        
        read_set_t read_set;
        write_set_t write_set;
        
        inline transaction_impl(transaction_domain& domain,
                                thread_data& tls_td,
                                const Alloc& alloc) noexcept
            : transaction(domain, tls_td)
            , read_set(alloc)
            , write_set(alloc)
        {}
        
        static void unlock_write_set(write_set_iter begin, write_set_iter end) noexcept {
            for (; begin != end; ++begin)
                unlock(begin->dest_var());
        }
        
        void commit_sort_writes() noexcept
        { std::sort(std::begin(write_set), std::end(write_set)); }
        
        void commit_remove_writes_from_reads() noexcept {
            // using the bloom filter seemed to be mostly unhelpful
            for (auto& write_elem : write_set) {
                // TODO: weird to have this here
                // typical usage patterns would probly be:
                //   read shared variable
                //   do stuff
                //   store result
                //   end transaction
                // transactions are pretty composable which might matter a little
                read_set.set_end(std::remove_if(std::begin(read_set),
                                                std::end(read_set),
                                                [&](const read_set_value_type& rsv) noexcept {
                                                    return rsv.is_src_var(write_elem.dest_var());
                                                }));
            }
        }
        
        bool commit_lock_writes() noexcept {
            commit_sort_writes();
            commit_remove_writes_from_reads();
            
            write_set_iter write_begin = std::begin(write_set);
            write_set_iter write_end = std::end(write_set);
            
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
            for (auto& read_set_vaue : read_set) {
                if (!rw_valid(read_set_vaue.src_var())) {
                    unlock_write_set(std::begin(write_set), std::end(write_set));
                    return false;
                }
            }
            return true;
        }
        
        // TODO: emplace_back can throw exceptions...
        void commit_publish(const gp_t write_version) noexcept {
            for (auto& write_set_value : write_set) {
                // TODO: possibly could reduce overhead here by adding to the write_set_deleters
                // in "store" calls
                if (write_set_value.dest_var().kind != var_type::atomic) {
                    write_set_deleter dler(&write_set_value.dest_var(),
                                           write_set_value.dest_var().storage.load(LSTM_RELAXED));
                    tls_td->gp_callbacks.emplace_back(std::move(dler));
                }
                write_set_value.dest_var().storage.store(std::move(write_set_value.pending_write()),
                                                         LSTM_RELAXED);
                unlock(write_version, write_set_value.dest_var());
            }
        }
        
        void commit_reclaim_slow_path() noexcept {
            tls_td->synchronize(read_version);
            tls_td->do_callbacks();
        }
        
        void commit_reclaim() noexcept {
            // TODO: batching
            if (!tls_td->gp_callbacks.empty())
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
            if ((!write_set.empty() || !tls_td->gp_callbacks.empty()) && !commit_slow_path()) {
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
            for (auto& write_set_value : write_set)
                write_set_value.dest_var().destroy_deallocate(write_set_value.pending_write());
            write_set.clear();
            read_set.clear();
            
            // TODO: batching
            tls_td->gp_callbacks.clear();
        }
        
        void reset_heap() noexcept {
            write_set.reset();
            read_set.reset();
        }
        
        void add_read_set(const var_base& src_var) override final
        { read_set.emplace_back(&src_var); }
        
        void add_write_set(var_base& dest_var,
                           const var_storage pending_write,
                           const hash_t hash) override final {
            // up to caller to ensure dest_var is not already in the write_set
            assert(!find_write_set(dest_var).success());
            write_set.push_back(&dest_var, pending_write, hash);
        }
        
        write_set_lookup find_write_set(const var_base& dest_var) noexcept override final {
            write_set_lookup lookup;
            lookup.hash = hash(dest_var);
            if (LSTM_LIKELY(!(write_set.filter() & lookup.hash)))
                return lookup;
            write_set_iter iter = slow_find(write_set.begin(), write_set.end(), dest_var);
            return iter != write_set.end()
                ? write_set_lookup{&iter->pending_write(), 0}
                : lookup;
        }
        
    public:
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
            LSTM_REQUIRES_(std::is_assignable<T&, U&&>() &&
                           std::is_constructible<T, U&&>())>
        void write(var<T, Alloc0>& dest_var, U&& u) {
            detail::write_set_lookup lookup = find_write_set(dest_var);
            if (LSTM_LIKELY(!lookup.success()))
                add_write_set(dest_var, dest_var.allocate_construct((U&&)u), lookup.hash);
            else
                var<T>::store(lookup.pending_write(), (U&&)u);
            
            if (!rw_valid(dest_var)) detail::internal_retry();
        }
        
        // reading/writing an rvalue probably never makes sense
        template<typename T, typename Alloc0> void read(var<T, Alloc0>&& v) = delete;
        template<typename T, typename Alloc0> void read(const var<T, Alloc0>&& v) = delete;
        template<typename T, typename Alloc0> void write(var<T, Alloc0>&& v) = delete;
        template<typename T, typename Alloc0> void write(const var<T, Alloc0>&& v) = delete;
    };
LSTM_DETAIL_END

#endif /* LSTM_TRANSACTION_HPP */
