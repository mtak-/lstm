#ifndef LSTM_DETAIL_THREAD_GP_NODE_HPP
#define LSTM_DETAIL_THREAD_GP_NODE_HPP

#include <lstm/detail/fast_rw_mutex.hpp>

LSTM_DETAIL_BEGIN
    using mutex_type = fast_rw_mutex;

    namespace
    {
        static_assert(std::is_unsigned<gp_t>{}, "");

        static constexpr gp_t lock_bit  = gp_t(1) << (sizeof(gp_t) * 8 - 1);
        static constexpr gp_t off_state = ~gp_t(0);
    }

    inline bool locked(const gp_t version) noexcept { return version & lock_bit; }
    inline gp_t as_locked(const gp_t version) noexcept { return version | lock_bit; }

    struct thread_gp_list_t
    {
        LSTM_CACHE_ALIGNED mutex_type mut{};
        thread_gp_node*               gp{nullptr};
    };

    LSTM_INLINE_VAR LSTM_CACHE_ALIGNED thread_gp_list_t thread_gp_list{};

    struct thread_gp_node
    {
    private:
        mutable mutex_type mut;
        char               padding2_[32 - sizeof(mutex_type)];
        thread_gp_node*    next;
        char               padding_[LSTM_CACHE_LINE_SIZE - sizeof(thread_gp_node*)];
        std::atomic<gp_t>  active;

        static void lock_all() noexcept
        {
            LSTM_ACCESS_INLINE_VAR(thread_gp_list).mut.lock();

            thread_gp_node* current = LSTM_ACCESS_INLINE_VAR(thread_gp_list).gp;
            while (current) {
                current->mut.lock();
                current = current->next;
            }
        }

        static void unlock_all() noexcept
        {
            thread_gp_node* current = LSTM_ACCESS_INLINE_VAR(thread_gp_list).gp;
            while (current) {
                current->mut.unlock();
                current = current->next;
            }

            LSTM_ACCESS_INLINE_VAR(thread_gp_list).mut.unlock();
        }

        static gp_t wait_min_gp(const gp_t gp) noexcept
        {
            gp_t result = off_state;
            for (thread_gp_node* q = LSTM_ACCESS_INLINE_VAR(thread_gp_list).gp; q != nullptr;
                 q                 = q->next) {
                default_backoff backoff;
                const gp_t      td_gp = q->active.load(LSTM_ACQUIRE);
                if (td_gp <= gp) {
                    do {
                        backoff();
                    } while (q->not_in_grace_period(gp));
                } else if (gp < result) {
                    result = gp;
                }
            }
            return result;
        }

    public:
        thread_gp_node() noexcept
        {
            (void)padding_;
            (void)padding2_;
            assert(std::uintptr_t(&mut) % LSTM_CACHE_LINE_SIZE == 0);
            assert(std::uintptr_t(&active) % LSTM_CACHE_LINE_SIZE == 0);

            active.store(off_state, LSTM_RELEASE);

            mut.lock();
            lock_all(); // this->mut does not get locked here
            {
                next = LSTM_ACCESS_INLINE_VAR(thread_gp_list).gp;
                LSTM_ACCESS_INLINE_VAR(thread_gp_list).gp = this;
            }
            unlock_all(); // this->mut gets unlocked here
        }

        thread_gp_node(const thread_gp_node&) = delete;
        thread_gp_node& operator=(const thread_gp_node&) = delete;

        ~thread_gp_node() noexcept
        {
            assert(!in_critical_section());

            lock_all(); // this->mut gets locked here
            {
                thread_gp_node** indirect = &LSTM_ACCESS_INLINE_VAR(thread_gp_list).gp;
                assert(*indirect);
                while (*indirect != this) {
                    indirect = &(*indirect)->next;
                    assert(*indirect);
                }
                *indirect = next;
            }
            unlock_all(); // this->mut does not get unlocked here
            mut.unlock();
        }

        inline gp_t gp() const noexcept { return active.load(LSTM_RELAXED); }

        inline bool in_critical_section() const noexcept
        {
            return active.load(LSTM_RELAXED) != off_state;
        }

        inline void access_lock(const gp_t gp) noexcept
        {
            assert(!in_critical_section());
            assert(gp != off_state);

            active.store(gp, LSTM_RELEASE);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }

        inline void access_relock(const gp_t gp) noexcept
        {
            assert(in_critical_section());
            assert(gp != off_state);
            assert(active.load(LSTM_RELAXED) <= gp);

            active.store(gp, LSTM_RELEASE);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }

        inline void access_unlock() noexcept
        {
            assert(in_critical_section());
            active.store(off_state, LSTM_RELEASE);
        }

        inline bool not_in_grace_period(const gp_t gp) const noexcept
        {
            // TODO: acquire seems unneeded
            return active.load(LSTM_ACQUIRE) <= gp;
        }

        // TODO: allow specifying a backoff strategy
        inline gp_t synchronize_min_gp(const gp_t gp) const noexcept
        {
            assert(!in_critical_section());
            assert(gp != off_state);

            gp_t result;

            mut.lock_shared();
            {
                result = wait_min_gp(gp);
            }
            mut.unlock_shared();

            return result;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_THREAD_GP_HPP */
