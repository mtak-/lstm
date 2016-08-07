#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>

#include <atomic>
#include <map>
#include <set>

LSTM_BEGIN
    struct transaction {
        template<typename Func>
        friend void atomic(const Func&);
    private:
        static std::atomic<word> clock;
        
        std::set<const detail::var_base*> read_set;
        std::map<detail::var_base*, void*> write_set;
        word version;
        
        transaction() noexcept : version(clock.load(LSTM_ACQUIRE)) {}
        
        bool lock(word& read_version, const detail::var_base& v) const noexcept {
            while (read_version <= version
                && !v.version_lock.compare_exchange_weak(read_version,
                                                         read_version | 1,
                                                         LSTM_RELEASE));
            return read_version <= version && !(read_version & 1);
        }
        
        void unlock(const word read_version, const detail::var_base& v) const noexcept
        { v.version_lock.store(read_version, LSTM_RELEASE); }
        
        void unlock(const detail::var_base& v) const noexcept
        { v.version_lock.fetch_sub(1, LSTM_RELEASE); }
        
        bool read_valid(const detail::var_base& v) const noexcept
        { return v.version_lock.load(LSTM_ACQUIRE) <= version; }
        
        void commit() {
            word read_version = 0;
            for (auto iter = std::begin(write_set); iter != std::end(write_set); ++iter) {
                word read_version = 0;
                if (!lock(read_version, *iter->first)) {
                    for (auto undo_iter = std::begin(write_set); undo_iter != iter; ++undo_iter)
                        unlock(*undo_iter->first);
                    throw tx_retry{};
                }
                read_set.erase(iter->first);
            }
            
            for (auto& var : read_set) {
                if (!read_valid(*var)) {
                    for (auto& var_val : write_set) {
                        unlock(*var_val.first);
                    }
                    throw tx_retry{};
                }
            }
                    
            word write_version = clock.fetch_add(2, LSTM_RELEASE) + 2;
            
            for (auto& var_val : write_set) {
                var_val.first->destroy_value();
                var_val.first->value.store(var_val.second, LSTM_RELEASE);
                // write_set
                unlock(write_version, *var_val.first);
            }
        }
        
    public:
        transaction(const transaction&) = delete;
        transaction(transaction&&) = delete;
        transaction& operator=(const transaction&) = delete;
        transaction& operator=(transaction&&) = delete;
        
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
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */