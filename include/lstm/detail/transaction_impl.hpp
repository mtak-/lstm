#ifndef LSTM_DETAIL_TRANSACTION_IMPL_HPP
#define LSTM_DETAIL_TRANSACTION_IMPL_HPP

#include <lstm/transaction.hpp>
#include <lstm/detail/small_pod_vector.hpp>

#include <urcu.h>

#include <algorithm>

LSTM_DETAIL_BEGIN
    struct write_set_deleter {
        var_base* var_ptr;
        var_storage storage;
        
        inline write_set_deleter() noexcept = default;
        inline constexpr write_set_deleter(var_base* const in_var_ptr,
                                           const var_storage in_storage) noexcept
            : var_ptr(in_var_ptr)
            , storage(in_storage)
        {}
    };

    struct read_set_value_type {
    private:
        const var_base* _src_var;
        
    public:
        inline read_set_value_type() noexcept = default;
        inline constexpr read_set_value_type(const var_base& in_src_var) noexcept
            : _src_var(&in_src_var)
        {}
            
        inline constexpr const var_base& src_var() const noexcept { return *_src_var; }
        
        inline constexpr bool is_src_var(const var_base& rhs) const noexcept
        { return _src_var == &rhs; }
        
        inline constexpr bool operator<(const read_set_value_type& rhs) const noexcept
        { return _src_var == rhs._src_var; }
    };
    
    struct write_set_value_type {
    private:
        var_base* _dest_var;
        var_storage _pending_write;
        
    public:
        inline write_set_value_type() noexcept = default;
        
        inline constexpr
        write_set_value_type(var_base& in_dest_var, var_storage in_pending_write) noexcept
            : _dest_var(&in_dest_var)
            , _pending_write(std::move(in_pending_write))
        {}
            
        inline constexpr var_base& dest_var() const noexcept { return *_dest_var; }
        inline constexpr var_storage& pending_write() noexcept { return _pending_write; }
        inline constexpr const var_storage& pending_write() const noexcept
        { return _pending_write; }
        
        inline constexpr bool is_dest_var(const var_base& rhs) const noexcept
        { return _dest_var == &rhs; }
        
        inline constexpr bool operator<(const write_set_value_type& rhs) const noexcept
        { return _dest_var < rhs._dest_var; }
    };
    
    template<typename Alloc>
    struct transaction_impl : lstm::transaction {
        friend detail::atomic_fn;
        friend test::transaction_tester;
    private:
        using alloc_traits = std::allocator_traits<Alloc>;
        using read_alloc = typename alloc_traits::template rebind_alloc<read_set_value_type>;
        using write_alloc = typename alloc_traits::template rebind_alloc<write_set_value_type>;
        using deleter_alloc = typename alloc_traits::template rebind_alloc<deleter_storage>;
        using write_deleter_alloc = typename alloc_traits::template rebind_alloc<write_set_deleter>;
        using read_set_t = small_pod_vector<read_set_value_type, 4, read_alloc>;
        using write_set_t = small_pod_vector<write_set_value_type, 4, write_alloc>;
        using deleter_set_t = small_pod_vector<deleter_storage, 4, deleter_alloc>;
        using write_set_deleters_t = small_pod_vector<write_set_deleter, 4, write_deleter_alloc>;
        using read_set_const_iter = typename read_set_t::const_iterator;
        using write_set_iter = typename write_set_t::iterator;

        // TODO: make some container choices
        read_set_t read_set;
        write_set_t write_set;
        deleter_set_t deleter_set;

        inline transaction_impl(transaction_domain* domain, const Alloc& alloc) noexcept
            : transaction(domain)
            , read_set(alloc)
            , write_set(alloc)
            , deleter_set(alloc)
        {}
            
        static void unlock_write_set(write_set_iter begin, write_set_iter end) noexcept {
            for (; begin != end; ++begin)
                unlock(begin->dest_var());
        }

        void commit_lock_writes() {
            // pretty sure write needs to be sorted
            // in order to have lockfreedom on trivial types?
            auto write_begin = std::begin(write_set);
            auto write_end = std::end(write_set);
            std::sort(write_begin, write_end);
            
            word version_buf;
            for (auto write_iter = write_begin; write_iter != write_end; ++write_iter) {
                version_buf = 0;
                // TODO: only care what version the var is, if it's also a read?
                if (!lock(version_buf, write_iter->dest_var())) {
                    unlock_write_set(std::move(write_begin), std::move(write_iter));
                    detail::internal_retry();
                }
                
                // TODO: weird to have this here
                auto read_iter = find_read_set(write_iter->dest_var());
                if (read_iter != std::end(read_set))
                    read_set.unordered_erase(read_iter);
            }
        }

        void commit_validate_reads() {
            // reads do not need to be locked to be validated
            for (auto& read_set_vaue : read_set) {
                if (!read_valid(read_set_vaue.src_var())) {
                    unlock_write_set(std::begin(write_set), std::end(write_set));
                    detail::internal_retry();
                }
            }
        }

        void commit_publish(const word write_version,
                            write_set_deleters_t& write_set_deleters) noexcept {
            for (auto& write_set_value : write_set) {
                if (write_set_value.dest_var().kind != var_type::atomic)
                    write_set_deleters.emplace_back(&write_set_value.dest_var(),
                                                    write_set_value.dest_var().storage);
                write_set_value.dest_var().storage = std::move(write_set_value.pending_write());
                unlock(write_version, write_set_value.dest_var());
            }
        }
        
        void commit_reclaim(const write_set_deleters_t& write_set_deleters) noexcept {
            synchronize_rcu();
            for (auto& dler : deleter_set)
                (*static_cast<deleter_base*>(static_cast<a_deleter_type*>((void*)&dler)))();
            for (auto& dler : write_set_deleters)
                dler.var_ptr->destroy_deallocate(dler.storage);
        }
        
        void commit_slow_path(write_set_deleters_t& write_set_deleters) {
            commit_lock_writes();
            auto write_version = domain().bump_clock();
            
            commit_validate_reads();
            
            commit_publish(write_version, write_set_deleters);
        }

        void commit() {
            write_set_deleters_t write_set_deleters;
            if (!write_set.empty())
                commit_slow_path(write_set_deleters);
            if (!deleter_set.empty() || !write_set_deleters.empty())
                commit_reclaim(write_set_deleters);
            LSTM_SUCC_TX();
        }

        void cleanup() noexcept {
            // TODO: destroy only???
            for (auto& write_set_value : write_set)
                write_set_value.dest_var().destroy_deallocate(write_set_value.pending_write());
            write_set.clear();
            read_set.clear();
            deleter_set.clear();
        }
        
        read_set_const_iter find_read_set(const var_base& src_var) const noexcept {
            return std::find_if(std::begin(read_set),
                                std::end(read_set),
                                [&src_var](const read_set_value_type& rhs) noexcept -> bool
                                { return rhs.is_src_var(src_var); });
        }

        void add_read_set(const var_base& src_var) override final {
            if (find_read_set(src_var) == std::end(read_set))
                read_set.emplace_back(src_var);
        }

        void add_write_set(var_base& dest_var, var_storage pending_write) override final {
            // up to caller to ensure dest_var is not already in the write_set
            assert(!find_write_set(dest_var).success());
            write_set.emplace_back(dest_var, std::move(pending_write));
        }

        write_set_lookup find_write_set(const var_base& dest_var) noexcept override final {
            const auto end = std::end(write_set);
            const auto iter = std::find_if(
                std::begin(write_set),
                end,
                [&dest_var](const write_set_value_type& rhs) noexcept -> bool
                { return rhs.is_dest_var(dest_var); });
            return iter != end
                ? write_set_lookup{iter->pending_write()}
                : write_set_lookup{nullptr};
        }
        
        deleter_storage& delete_set_push_back_storage() override final {
            deleter_set.emplace_back();
            return deleter_set.back();
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_TRANSACTION_HPP */