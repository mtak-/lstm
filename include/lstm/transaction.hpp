#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>

#include <atomic>
#include <cassert>
#include <map>
#include <set>

LSTM_BEGIN
    namespace detail {
        struct transaction_base {
        protected:
            friend detail::atomic_fn;
            friend test::transaction_tester;
            
            template<std::nullptr_t = nullptr>
            static std::atomic<word> clock;
            static inline word get_clock() noexcept { return clock<>.load(LSTM_ACQUIRE); }
            
            word version;
            
            transaction_base() noexcept = default;
            ~transaction_base() noexcept = default;
            
            bool lock(word& version_buf, const detail::var_base& v) const noexcept {
                // TODO: this feels wrong, especially since reads call this...
                // reader writer style lock might be more appropriate, but that'd probly starve
                // writes
                while (version_buf <= version &&
                    !v.version_lock.compare_exchange_weak(version_buf,
                                                          version_buf | 1,
                                                          LSTM_RELEASE)); // TODO: is this correct?
                return version_buf <= version && !(version_buf & 1);
            }
            
            static void unlock(const word read_version, const detail::var_base& v) noexcept {
                assert((v.version_lock.load(LSTM_RELAXED) & 1) == 1);
                assert((read_version & 1) == 0);
                v.version_lock.store(read_version, LSTM_RELEASE);
            }
            
            static void unlock(const detail::var_base& v) noexcept {
                assert((v.version_lock.load(LSTM_RELAXED) & 1) == 1);
                v.version_lock.fetch_sub(1, LSTM_RELEASE);
            }
            
            bool read_valid(const detail::var_base& v) const noexcept
            { return v.version_lock.load(LSTM_ACQUIRE) <= version; }
            
            virtual void add_read_set(const detail::var_base& v) = 0;
            virtual void add_write_set(detail::var_base& v, void* ptr) = 0;
            virtual void* find_write_set(detail::var_base& v) noexcept = 0;
            
        public:
            transaction_base(const transaction_base&) = delete;
            transaction_base(transaction_base&&) = delete;
            transaction_base& operator=(const transaction_base&) = delete;
            transaction_base& operator=(transaction_base&&) = delete;
            
            template<typename T>
            T load(const var<T>& v) {
                void* write_ptr = find_write_set(const_cast<var<T>&>(v));
                if (!write_ptr) {
                    word read_version = 0;
                    if (lock(read_version, v)) { // TODO: not sure locking best possible solution
                        // if copying T causes a lock to be taken out on T, then this line will
                        // abort the transaction or cause a stack overflow.
                        // this is ok, because it is certainly a bug in the client code
                        // (me thinks..)
                        auto result = *(T*)v.value;
                        unlock(read_version, v);
                        
                        add_read_set(v);
                        return result;
                    }
                } else if (read_valid(v)) // TODO: does this if check improve or hurt speed?
                    return *static_cast<T*>(write_ptr);
                throw tx_retry{};
            }
            
            template<typename T, typename U,
                LSTM_REQUIRES_(std::is_assignable<T&, U&&>() &&
                               std::is_constructible<T, U&&>())>
            void store(var<T>& v, U&& u) __attribute__((noinline)){
                auto write_ptr = find_write_set(v);
                if (!write_ptr)
                    add_write_set(v, (void*)v.allocate_construct((U&&)u));
                else
                    *static_cast<T*>(write_ptr) = (U&&)u;
                
                // TODO: where is this best placed???, or is it best removed altogether?
                if (!read_valid(v)) throw tx_retry{};
            }
            
            // TODO: reading/writing an rvalue probably never makes sense?
            template<typename T>
            void load(const var<T>&& v) = delete;
            
            template<typename T>
            void store(var<T>&& v) = delete;
        };
        
        template<std::nullptr_t>
        std::atomic<word> transaction_base::clock{0};
    }
    
    template<typename Alloc>
    struct transaction : detail::transaction_base {
    private:
        friend detail::atomic_fn;
        friend test::transaction_tester;
        
        std::set<const detail::var_base*, std::less<>, Alloc> read_set;
        std::map<detail::var_base*, void*, std::less<>, Alloc> write_set;
        
        transaction(const Alloc& alloc) // TODO: noexcept ?
            : read_set(alloc)
            , write_set(alloc)
        {}
        
        ~transaction() noexcept = default;
        
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
            // reads do not need to be locked to be validated
            for (auto& var : read_set) {
                if (!read_valid(*var)) {
                    for (auto& var_val : write_set)
                        unlock(*var_val.first);
                    throw tx_retry{};
                }
            }
        }
        
        void commit_publish(const word write_version) {
            for (auto& var_val : write_set) {
                var_val.first->destroy_deallocate(var_val.first->value);
                var_val.first->value = var_val.second;
                unlock(write_version, *var_val.first);
            }
            
            write_set.clear();
        }
        
        void commit() {
            commit_lock_writes();
            auto write_version = clock<>.fetch_add(2, LSTM_RELEASE) + 2;
            commit_validate_reads();
            commit_publish(write_version);
        }
        
        void cleanup() noexcept {
            for (auto& var_val : write_set)
                var_val.first->destroy_deallocate(var_val.second);
            write_set.clear();
        }
        
        void add_read_set(const detail::var_base& v) override final
        { read_set.emplace(std::addressof(v)); }
        
        void add_write_set(detail::var_base& v, void* ptr) override final
        { write_set.emplace(std::addressof(v), ptr); }
        
        void* find_write_set(detail::var_base& v) noexcept override final {
            auto iter = write_set.find(std::addressof(v));
            return iter != std::end(write_set) ? iter->second : nullptr;
        }
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */