#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>

#include <atomic>
#include <cassert>

LSTM_BEGIN
    [[noreturn]] inline void retry() { throw detail::tx_retry{}; }

    struct transaction {
    protected:
        friend detail::atomic_fn;
        friend test::transaction_tester;

        template<std::nullptr_t = nullptr> static std::atomic<word> clock;
        static inline word get_clock() noexcept { return clock<>.load(LSTM_ACQUIRE); }

        word read_version;

        inline transaction() noexcept : read_version(get_clock()) {}
        
        static inline bool locked(word version) noexcept { return version & 1; }
        static inline word as_locked(word version) noexcept { return version | 1; }

        bool lock(word& version_buf, const detail::var_base& v) const noexcept {
            while (version_buf <= read_version &&
                !v.version_lock.compare_exchange_weak(version_buf,
                                                      as_locked(version_buf),
                                                      LSTM_RELEASE)); // TODO: is this correct?
            return version_buf <= read_version && !locked(version_buf);
        }

        static inline
        void unlock(const word version_to_set, const detail::var_base& v) noexcept {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            assert(!locked(version_to_set));
            v.version_lock.store(version_to_set, LSTM_RELEASE);
        }

        static inline void unlock(const detail::var_base& v) noexcept {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            v.version_lock.fetch_sub(1, LSTM_RELEASE); // TODO: seems correct, but might not be
        }

        inline bool read_valid(const detail::var_base& v) const noexcept
        { return v.version_lock.load(LSTM_ACQUIRE) <= read_version; }

        virtual void add_read_set(const detail::var_base& src_var) = 0;
        virtual void add_write_set(detail::var_base& dest_var,
                                   detail::var_storage pending_write) = 0;
        
        virtual detail::write_set_lookup
        find_write_set(const detail::var_base& dest_var) noexcept = 0;

    public:
        // transactions can only be passed by non-const lvalue reference
        transaction(const transaction&) = delete;
        transaction(transaction&&) = delete;
        transaction& operator=(const transaction&) = delete;
        transaction& operator=(transaction&&) = delete;
        
        // non trivial loads, require locking the var<T> before copying it
        template<typename T,
            LSTM_REQUIRES_(!var<T>::trivial)>
        T load(const var<T>& src_var) {
            detail::write_set_lookup lookup = find_write_set(src_var);
            if (!lookup.success()) {
                word version_buf = 0;
                // TODO: this feels wrong, especially since reads call this...
                if (lock(version_buf, src_var)) {
                    // if copying T causes a lock to be taken out on T, then this line will
                    // abort the transaction or cause a stack overflow.
                    // this is ok, because it is certainly a bug in the client code
                    // (me thinks..)
                    T result = src_var.unsafe();
                    unlock(version_buf, src_var);

                    add_read_set(src_var);
                    return result;
                }
            } else if (read_valid(src_var)) // TODO: does this if check improve or hurt speed?
                return var<T>::load(lookup.pending_write());
            retry();
        }
        
        // trivial loads are fast :)
        template<typename T,
            LSTM_REQUIRES_(var<T>::trivial)>
        T load(const var<T>& src_var) {
            detail::write_set_lookup lookup = find_write_set(src_var);
            if (!lookup.success()) {
                // TODO: is this synchronization correct?
                // or does it thrash the cache too much?
                auto src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                if (src_version <= read_version && !locked(src_version)) {
                    T result = var<T>::load(src_var.storage);
                    
                    if (src_var.version_lock.load(LSTM_ACQUIRE) == src_version) {
                        add_read_set(src_var);
                        return result;
                    }
                }
            } else if (read_valid(src_var)) // TODO: does this if check improve or hurt speed?
                return var<T>::load(lookup.pending_write());
            retry();
        }

        template<typename T, typename U,
            LSTM_REQUIRES_(std::is_assignable<T&, U&&>() &&
                           std::is_constructible<T, U&&>())>
        void store(var<T>& dest_var, U&& u) {
            detail::write_set_lookup lookup = find_write_set(dest_var);
            if (!lookup.success())
                add_write_set(dest_var, dest_var.allocate_construct((U&&)u));
            else
                dest_var.load(lookup.pending_write()) = (U&&)u;

            // TODO: where is this best placed???, or is it best removed altogether?
            if (!read_valid(dest_var)) retry();
        }

        // TODO: reading/writing an rvalue probably never makes sense?
        template<typename T>
        void load(const var<T>&& v) = delete;

        template<typename T>
        void store(var<T>&& v) = delete;
    };

    template<std::nullptr_t> std::atomic<word> transaction::clock{0};
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */
