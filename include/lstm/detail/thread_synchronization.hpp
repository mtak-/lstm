#ifndef LSTM_DETAIL_THREAD_SYNCHRONIZATION_HPP
#define LSTM_DETAIL_THREAD_SYNCHRONIZATION_HPP

#include <lstm/detail/fast_rw_mutex.hpp>
#include <lstm/detail/pod_vector.hpp>

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
        LSTM_CACHE_ALIGNED mutex_type                                   mut{};
        pod_vector<const thread_synchronization_node<CacheLineOffset>*> nodes;
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

        static void lock_all() noexcept
        {
            global_synchronization_list<CacheLineOffset>.mut.lock();

            for (const thread_synchronization_node* node :
                 global_synchronization_list<CacheLineOffset>.nodes)
                node->mut.lock();
        }

        static void unlock_all() noexcept
        {
            for (const thread_synchronization_node* node :
                 global_synchronization_list<CacheLineOffset>.nodes)
                node->mut.unlock();

            global_synchronization_list<CacheLineOffset>.mut.unlock();
        }

        LSTM_NOINLINE static void
        wait_on_epoch(const epoch_t epoch, const thread_synchronization_node* const node) noexcept
        {
            default_backoff backoff;
            do {
                backoff();
            } while (node->epoch_less_equal_to(epoch));
        }

        static epoch_t wait_min_epoch(const epoch_t epoch) noexcept
        {
            epoch_t result = off_state;
            for (const thread_synchronization_node* node :
                 global_synchronization_list<CacheLineOffset>.nodes) {
                const epoch_t td_epoch = node->active.load(LSTM_ACQUIRE);

                if (LSTM_UNLIKELY(td_epoch <= epoch))
                    wait_on_epoch(epoch, node);
                else if (td_epoch < result)
                    result = td_epoch;
            }

            LSTM_PERF_STATS_QUIESCES();

            return result;
        }

    public:
        thread_synchronization_node() noexcept
            : mut()
            , active(off_state)
        {
            LSTM_ASSERT(std::uintptr_t(this) % LSTM_CACHE_LINE_SIZE == CacheLineOffset);
            LSTM_ASSERT(std::uintptr_t(&active) % LSTM_CACHE_LINE_SIZE == 0);

            mut.lock();
            lock_all(); // this->mut does not get locked here
            {
                global_synchronization_list<CacheLineOffset>.nodes.emplace_back(this);
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
                for (const thread_synchronization_node*& node :
                     global_synchronization_list<CacheLineOffset>.nodes) {
                    LSTM_ASSERT(node);
                    if (node == this) {
                        global_synchronization_list<CacheLineOffset>.nodes.unordered_erase(&node);
                        break;
                    }
                }
                LSTM_PERF_STATS_PUBLISH_RECORD();
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
