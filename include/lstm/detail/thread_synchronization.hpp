#ifndef LSTM_DETAIL_THREAD_SYNCHRONIZATION_HPP
#define LSTM_DETAIL_THREAD_SYNCHRONIZATION_HPP

#include <lstm/detail/fast_rw_mutex.hpp>

LSTM_DETAIL_BEGIN
    using mutex_type = fast_rw_mutex;

    namespace
    {
        static_assert(std::is_integral<epoch_t>{}, "");
        static_assert(std::is_unsigned<epoch_t>{}, "");

        static constexpr epoch_t lock_bit  = epoch_t(1) << (sizeof(epoch_t) * 8 - 1);
        static constexpr epoch_t off_state = ~epoch_t(0);
    }

    inline bool locked(const epoch_t version) noexcept { return version & lock_bit; }
    inline epoch_t as_locked(const epoch_t version) noexcept { return version | lock_bit; }

    template<std::size_t CacheLineOffset>
    struct thread_synchronization_list
    {
        LSTM_CACHE_ALIGNED mutex_type                 mut{};
        thread_synchronization_node<CacheLineOffset>* root{nullptr};
    };

    template<std::size_t                         CacheLineOffset>
    thread_synchronization_list<CacheLineOffset> global_synchronization_list{};

    template<std::size_t CacheLineOffset>
    struct thread_synchronization_node
    {
    private:
        static_assert(CacheLineOffset < LSTM_CACHE_LINE_SIZE, "");
        static constexpr std::size_t remaining_bytes = LSTM_CACHE_LINE_SIZE - CacheLineOffset;
        static_assert(remaining_bytes % alignof(mutex_type) == 0, "probly breaks on some platform");
        static_assert(sizeof(mutex_type) == alignof(mutex_type), "");

        union
        {
            mutable mutex_type mut;
            char               padding_[remaining_bytes >= sizeof(mutex_type) ? remaining_bytes
                                                                : LSTM_CACHE_LINE_SIZE];
        };

        union
        {
            std::atomic<epoch_t> active;
            char                 padding3_[LSTM_CACHE_LINE_SIZE];
        };

        union
        {
            thread_synchronization_node* next;
            char                         padding2_[LSTM_CACHE_LINE_SIZE];
        };

        static void lock_all() noexcept
        {
            global_synchronization_list<CacheLineOffset>.mut.lock();

            thread_synchronization_node* current
                = global_synchronization_list<CacheLineOffset>.root;
            while (current) {
                current->mut.lock();
                current = current->next;
            }
        }

        static void unlock_all() noexcept
        {
            thread_synchronization_node* current
                = global_synchronization_list<CacheLineOffset>.root;
            while (current) {
                current->mut.unlock();
                current = current->next;
            }

            global_synchronization_list<CacheLineOffset>.mut.unlock();
        }

        LSTM_NOINLINE_LUKEWARM static void
        wait_on_epoch(const epoch_t epoch, const thread_synchronization_node* const q) noexcept
        {
            default_backoff backoff;
            do {
                backoff();
            } while (q->epoch_less_equal_to(epoch));
        }

        static epoch_t wait_min_epoch(const epoch_t epoch) noexcept
        {
            epoch_t result = off_state;
            for (const thread_synchronization_node* q
                 = global_synchronization_list<CacheLineOffset>.root;
                 q != nullptr;
                 q = q->next) {
                const epoch_t td_epoch = q->active.load(LSTM_ACQUIRE);

                if (LSTM_UNLIKELY(td_epoch <= epoch))
                    wait_on_epoch(epoch, q);
                else if (td_epoch < result)
                    result = td_epoch;
            }

            LSTM_LOG_QUIESCES();

            return result;
        }

    public:
        thread_synchronization_node() noexcept
            : mut()
            , active(off_state)
        {
            LSTM_ASSERT(std::uintptr_t(this) % LSTM_CACHE_LINE_SIZE == CacheLineOffset);
            LSTM_ASSERT(std::uintptr_t(&next) % LSTM_CACHE_LINE_SIZE == 0);
            LSTM_ASSERT(std::uintptr_t(&active) % LSTM_CACHE_LINE_SIZE == 0);

            mut.lock();
            lock_all(); // this->mut does not get locked here
            {
                next = global_synchronization_list<CacheLineOffset>.root;
                global_synchronization_list<CacheLineOffset>.root = this;
            }
            unlock_all(); // this->mut gets unlocked here
        }

        thread_synchronization_node(const thread_synchronization_node&) = delete;
        thread_synchronization_node& operator=(const thread_synchronization_node&) = delete;

        ~thread_synchronization_node() noexcept
        {
            LSTM_ASSERT(!in_critical_section());

            lock_all(); // this->mut gets locked here
            {
                thread_synchronization_node** indirect
                    = &global_synchronization_list<CacheLineOffset>.root;
                LSTM_ASSERT(*indirect);
                while (*indirect != this) {
                    indirect = &(*indirect)->next;
                    LSTM_ASSERT(*indirect);
                }
                *indirect = next;

                LSTM_LOG_PUBLISH_RECORD();
            }
            unlock_all(); // this->mut does not get unlocked here
            mut.unlock();
        }

        inline epoch_t epoch() const noexcept { return active.load(LSTM_RELAXED); }

        inline bool in_critical_section() const noexcept
        {
            return active.load(LSTM_RELAXED) != off_state;
        }

        inline void access_lock(const epoch_t epoch) noexcept
        {
            LSTM_ASSERT(!in_critical_section());
            LSTM_ASSERT(epoch != off_state);

            active.store(epoch, LSTM_RELEASE);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }

        inline void access_relock(const epoch_t epoch) noexcept
        {
            LSTM_ASSERT(in_critical_section());
            LSTM_ASSERT(epoch != off_state);
            LSTM_ASSERT(active.load(LSTM_RELAXED) <= epoch);

            active.store(epoch, LSTM_RELEASE);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }

        inline void access_unlock() noexcept
        {
            LSTM_ASSERT(in_critical_section());
            active.store(off_state, LSTM_RELEASE);
        }

        bool epoch_less_equal_to(const epoch_t epoch) const noexcept
        {
            // TODO: acquire seems unneeded
            return active.load(LSTM_ACQUIRE) <= epoch;
        }

        // TODO: allow specifying a backoff strategy
        epoch_t synchronize_min_epoch(const epoch_t epoch) const noexcept
        {
            LSTM_ASSERT(!in_critical_section());
            LSTM_ASSERT(epoch != off_state);

            epoch_t result;

            mut.lock_shared();
            {
                result = wait_min_epoch(epoch);
            }
            mut.unlock_shared();

            return result;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_THREAD_SYNCHRONIZATION_HPP */
