#ifndef LSTM_THREAD_DATA_HPP
#define LSTM_THREAD_DATA_HPP

#include <lstm/detail/backoff.hpp>
#include <lstm/detail/gp_callback.hpp>
#include <lstm/detail/var_detail.hpp>
#include <lstm/detail/pod_hash_set.hpp>
#include <lstm/detail/pod_vector.hpp>
#include <lstm/detail/read_set_value_type.hpp>
#include <lstm/detail/write_set_value_type.hpp>

LSTM_DETAIL_BEGIN
    namespace { static constexpr gp_t off_state = ~gp_t(0); }
    
    struct global_data {
        mutex_type thread_data_mut{};
        thread_data* thread_data_root{nullptr};
    };
    
    LSTM_INLINE_VAR LSTM_CACHE_ALIGNED global_data globals{};

    LSTM_NOINLINE inline thread_data& tls_data_init() noexcept;
LSTM_DETAIL_END

LSTM_BEGIN
    LSTM_ALWAYS_INLINE thread_data& tls_thread_data() noexcept;
    
    // with the guarantee of no nested critical sections only one bit is needed
    // to say a thread is active.
    // this means the remaining bits can be used for the grace period, resulting
    // in concurrent writes
    struct LSTM_CACHE_ALIGNED thread_data {
    private:
        friend struct detail::read_write_fn;
        friend transaction;
        friend LSTM_NOINLINE inline thread_data& detail::tls_data_init() noexcept;
        
        LSTM_CACHE_ALIGNED mutex_type mut;
        LSTM_CACHE_ALIGNED thread_data* next;
        transaction* tx;
        
        // TODO: this is a terrible type for gp_callbacks
        // also probly should be a few different kinds of callbacks (succ/fail/always)
        // sharing of gp_callbacks would be nice, but how could it be made fast for small tx's?
        detail::pod_vector<detail::gp_callback> gp_callbacks;
        detail::pod_vector<detail::gp_callback> fail_callbacks;
        LSTM_CACHE_ALIGNED std::atomic<gp_t> active;
        
        using read_set_t = detail::pod_vector<detail::read_set_value_type>;
        using write_set_t = detail::pod_hash_set<detail::pod_vector<detail::write_set_value_type>>;
        
        read_set_t read_set;
        write_set_t write_set;
        
        static void lock_all() noexcept {
            LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_mut.lock();
            
            thread_data* current = LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
            while (current) {
                current->mut.lock();
                current = current->next;
            }
        }
        
        static void unlock_all() noexcept {
            thread_data* current = LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
            while (current) {
                current->mut.unlock();
                current = current->next;
            }
            
            LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_mut.unlock();
        }
        
        // TODO: allow specifying a backoff strategy
        static void wait(const gp_t gp) noexcept {
            for (thread_data* q = LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
                    q != nullptr;
                    q = q->next) {
                detail::default_backoff backoff;
                while (q->not_in_grace_period(gp))
                    backoff();
            }
        }
        
        LSTM_NOINLINE thread_data() noexcept
            : tx(nullptr)
        {
            active.store(detail::off_state, LSTM_RELEASE);
            
            mut.lock();
            
            lock_all(); // this->mut does not get locked here
                
                next = LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
                LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root = this;
                
            unlock_all(); // this->mut gets unlocked here
        }
        
        LSTM_NOINLINE ~thread_data() noexcept {
            assert(tx == nullptr);
            assert(active.load(LSTM_RELAXED) == detail::off_state);
            
            lock_all(); // this->mut gets locked here
                
                thread_data** indirect = &LSTM_ACCESS_INLINE_VAR(detail::globals).thread_data_root;
                assert(*indirect);
                while (*indirect != this) {
                    indirect = &(*indirect)->next;
                    assert(*indirect);
                }
                *indirect = next;
                
            unlock_all(); // this->mut does not get unlocked here
            
            mut.unlock();
            
            gp_callbacks.reset();
            fail_callbacks.reset();
            read_set.reset();
            write_set.reset();
        }
        
        thread_data(const thread_data&) = delete;
        thread_data& operator=(const thread_data&) = delete;
        
        // TODO: when atomic swap on gp_callbacks is possible, this needs to do just that
        void do_callbacks() noexcept {
            // TODO: if a callback adds a callback, this fails, again need a different type
            for (auto& gp_callback : gp_callbacks)
                gp_callback();
            gp_callbacks.clear();
        }
        
        // TODO: when atomic swap on gp_callbacks is possible, this needs to do just that
        void do_fail_callbacks() noexcept {
            // TODO: if a callback adds a callback, this fails, again need a different type
            for (auto& fail_callback : fail_callbacks)
                fail_callback();
            fail_callbacks.clear();
        }
        
        template<typename Alloc, typename Pointer>
        void queue_allocate_rollback(Alloc& alloc, Pointer ptr) {
            using alloc_traits = std::allocator_traits<Alloc>;
            fail_callbacks.emplace_back([alloc = &alloc, ptr = std::move(ptr)] {
                alloc_traits::deallocate(*alloc, std::move(ptr), 1);
            });
        }
        
        template<typename Alloc, typename Pointer>
        void queue_allocate_rollback(Alloc& alloc, Pointer ptr, const std::size_t count) {
            using alloc_traits = std::allocator_traits<Alloc>;
            fail_callbacks.emplace_back([alloc = &alloc, ptr = std::move(ptr), count] {
                alloc_traits::deallocate(*alloc, std::move(ptr), count);
            });
        }
        
        template<typename Alloc, typename Pointer>
        void queue_deallocate(Alloc& alloc, Pointer ptr) {
            using alloc_traits = std::allocator_traits<Alloc>;
            gp_callbacks.emplace_back([alloc = &alloc, ptr = std::move(ptr)] {
                alloc_traits::deallocate(*alloc, std::move(ptr), 1);
            });
        }
        
        template<typename Alloc, typename Pointer>
        void queue_deallocate(Alloc& alloc, Pointer ptr, const std::size_t count) {
            using alloc_traits = std::allocator_traits<Alloc>;
            gp_callbacks.emplace_back([alloc = &alloc, ptr = std::move(ptr), count] {
                alloc_traits::deallocate(*alloc, std::move(ptr), count);
            });
        }
        
    public:
        inline bool in_transaction() const { return tx != nullptr; }
        inline bool in_critical_section() const
        { return active.load(LSTM_RELAXED) != detail::off_state; }
        
        template<typename Alloc,
                 typename AllocTraits = std::allocator_traits<Alloc>,
                 typename Pointer = typename AllocTraits::pointer,
            LSTM_REQUIRES_(!std::is_const<Alloc>{})>
        Pointer allocate(Alloc& alloc) {
            Pointer result = AllocTraits::allocate(alloc, 1);
            if (in_transaction())
                queue_allocate_rollback(alloc, result);
            return result;
        }
        
        template<typename Alloc,
                 typename AllocTraits = std::allocator_traits<Alloc>,
                 typename Pointer = typename AllocTraits::pointer,
            LSTM_REQUIRES_(!std::is_const<Alloc>{})>
        Pointer allocate(Alloc& alloc, const std::size_t count) {
            Pointer result = AllocTraits::allocate(alloc, count);
            if (in_transaction())
                queue_allocate_rollback(alloc, result, count);
            return result;
        }
        
        template<typename Alloc,
                 typename AllocTraits = std::allocator_traits<Alloc>,
            LSTM_REQUIRES_(!std::is_const<Alloc>{})>
        void deallocate(Alloc& alloc, typename AllocTraits::pointer& ptr) {
            if (in_transaction())
                queue_deallocate(alloc, ptr);
            else
                AllocTraits::deallocate(alloc, ptr, 1);
        }
        
        template<typename Alloc,
                 typename AllocTraits = std::allocator_traits<Alloc>,
            LSTM_REQUIRES_(!std::is_const<Alloc>{})>
        void deallocate(Alloc& alloc, typename AllocTraits::pointer& ptr, const std::size_t count) {
            if (in_transaction())
                queue_deallocate(alloc, ptr, count);
            else
                AllocTraits::deallocate(alloc, ptr, count);
        }
        
        inline void access_lock(const gp_t gp) noexcept {
            assert(active.load(LSTM_RELAXED) == detail::off_state);
            assert(gp != detail::off_state);
            active.store(gp, LSTM_RELEASE);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }
        
        inline void access_relock(const gp_t gp) noexcept {
            assert(active.load(LSTM_RELAXED) != detail::off_state);
            assert(gp != detail::off_state);
            active.store(gp, LSTM_RELEASE);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }
        
        inline void access_unlock() noexcept {
            assert(active.load(LSTM_RELAXED) != detail::off_state);
            active.store(detail::off_state, LSTM_RELEASE);
        }
        
        inline bool not_in_grace_period(const gp_t gp) const noexcept {
            // TODO: acquire seems unneeded
            return active.load(LSTM_ACQUIRE) <= gp;
        }
        
        // TODO: allow specifying a backoff strategy
        inline void synchronize(const gp_t gp) noexcept {
            assert(tls_thread_data().active.load(LSTM_RELAXED) == detail::off_state);
            assert(&tls_thread_data().mut == &mut);
            assert(gp != detail::off_state);
            
            mut.lock();
            
                wait(gp);
            
            mut.unlock();
        }
    };
    
    namespace detail {
        LSTM_INLINE_VAR LSTM_THREAD_LOCAL thread_data* _tls_thread_data_ptr = nullptr;
        
        // TODO: still feel like this garbage is overkill, maybe this only applies to darwin
        LSTM_NOINLINE inline thread_data& tls_data_init() noexcept {
            static LSTM_THREAD_LOCAL thread_data _tls_thread_data{};
            LSTM_ACCESS_INLINE_VAR(_tls_thread_data_ptr) = &_tls_thread_data;
            return *LSTM_ACCESS_INLINE_VAR(_tls_thread_data_ptr);
        }
    }
    
    LSTM_ALWAYS_INLINE thread_data& tls_thread_data() noexcept {
        if (LSTM_UNLIKELY(!LSTM_ACCESS_INLINE_VAR(detail::_tls_thread_data_ptr)))
            return detail::tls_data_init();
        return *LSTM_ACCESS_INLINE_VAR(detail::_tls_thread_data_ptr);
    }
    
    static_assert(std::is_standard_layout<thread_data>{}, "");
LSTM_END

#endif /* LSTM_THREAD_DATA_HPP */
