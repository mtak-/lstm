#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>

#include <atomic>
#include <map>
#include <set>

LSTM_BEGIN
    template<typename Alloc>
    struct transaction : private Alloc {
        friend detail::atomic_fn;
    private:
        static std::atomic<word> clock;
        
        std::set<const detail::var_base*, std::less<>, Alloc> read_set;
        std::map<detail::var_base*, void*, std::less<>, Alloc> write_set;
        word version;
        
        transaction(Alloc alloc) noexcept
            : Alloc(std::move(alloc))
            , read_set(static_cast<Alloc&>(*this))
            , write_set(static_cast<Alloc&>(*this))
            , version(clock.load(LSTM_ACQUIRE))
        {}
        
        bool lock(word& version_buf, const detail::var_base& v) const noexcept {
            while (version_buf <= version
                && !v.version_lock.compare_exchange_weak(version_buf,
                                                         version_buf | 1,
                                                         LSTM_RELEASE)); // TODO: is this correct?
            return version_buf <= version && !(version_buf & 1);
        }
        
        void unlock(const word read_version, const detail::var_base& v) const noexcept
        { v.version_lock.store(read_version, LSTM_RELEASE); }
        
        void unlock(const detail::var_base& v) const noexcept
        { v.version_lock.fetch_sub(1, LSTM_RELEASE); }
        
        bool read_valid(const detail::var_base& v) const noexcept
        { return v.version_lock.load(LSTM_ACQUIRE) <= version; }
        
        void commit_lock_writes() {
            auto write_begin = std::begin(write_set);
            word write_version;
            for (auto iter = write_begin; iter != std::end(write_set); ++iter) {
                write_version = 0;
                // TODO: only care what version the var is, if it's also a read
                if (!lock(write_version, *iter->first)) {
                    for (; write_begin != iter; ++write_begin)
                        unlock(*write_begin->first);
                    throw tx_retry{};
                }
                read_set.erase(iter->first); // FIXME: weird to have this here
            }
        }
        
        void commit_validate_reads() {
            for (auto& var : read_set) {
                if (!read_valid(*var)) {
                    for (auto& var_val : write_set)
                        unlock(*var_val.first);
                    throw tx_retry{};
                }
            }
        }
        
        void commit_writes(const word write_version) {
            for (auto& var_val : write_set) {
                var_val.first->destroy_value(var_val.first->value.load(LSTM_RELAXED));
                var_val.first->value.store(var_val.second, LSTM_RELEASE);
                unlock(write_version, *var_val.first);
            }
            
            write_set.clear();
        }
        
        void commit() {
            commit_lock_writes();
            
            commit_validate_reads();
            
            commit_writes(clock.fetch_add(2, LSTM_RELEASE) + 2);
        }
        
    public:
        transaction(const transaction&) = delete;
        transaction(transaction&&) = delete;
        transaction& operator=(const transaction&) = delete;
        transaction& operator=(transaction&&) = delete;
        
        ~transaction() {
            for (auto& var_val : write_set)
                var_val.first->destroy_value(var_val.second);
        }
        
        template<typename T>
        T load(const var<T>& v) {
            auto write_iter = write_set.find(const_cast<var<T>*>(&v));
            if (write_iter == std::end(write_set)) {
                word read_version = 0;
                if (lock(read_version, v)) { // TODO: not sure locking is the best that can be done
                    auto result = *(T*)v.value.load(LSTM_RELAXED);
                    unlock(read_version, v);
                    
                    read_set.emplace(&v);
                    return result;
                }
            } else if (read_valid(v))
                return *static_cast<T*>(write_iter->second);
            throw tx_retry{};
        }
        
        template<typename T, typename U,
            LSTM_REQUIRES_(std::is_assignable<T&, U&&>() &&
                           std::is_constructible<T, U&&>())>
        void store(var<T>& v, U&& u) {
            auto write_iter = write_set.find(&v);
            if (write_iter != std::end(write_set))
                *static_cast<T*>(write_iter->second) = (U&&)u;
            else
                write_set.emplace(&v, new T((U&&)u));
            
            if (!read_valid(v))
                throw tx_retry{};
        }
        
        // reading/writing an rvalue probably never makes sense?
        template<typename T>
        void load(const var<T>&& v) = delete;
        
        template<typename T>
        void store(var<T>&& v) = delete;
    };
    
    template<typename Alloc>
    std::atomic<word> transaction<Alloc>::clock{0};
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */