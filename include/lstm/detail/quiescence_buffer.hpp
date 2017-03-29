#ifndef LSTM_DETAIL_QUIESCENCE_BUFFER_HPP
#define LSTM_DETAIL_QUIESCENCE_BUFFER_HPP

#include <lstm/detail/backoff.hpp>
#include <lstm/detail/gp_callback.hpp>
#include <lstm/detail/pod_vector.hpp>

LSTM_DETAIL_BEGIN
    struct quiescence_header
    {
        epoch_t epoch;
        uword   size;
        uword   prev_size;
    };

    union quiescence_buf_elem
    {
        quiescence_header header;
        gp_callback       callback;
    };

    inline constexpr bool is_power_of_two(const uword u) noexcept
    {
        return u && (u & (u - 1)) == 0;
    }

    // this class never calls construct/destroy... there's no need for POD types
    // if an allocator "requires" construct/destroy to be called, it will be in for
    // a surprise
    template<uword ReclaimLimit = 1024, typename Alloc = pod_mallocator<quiescence_buf_elem>>
    struct quiescence_buffer : private Alloc
    {
        using allocator_type  = Alloc;
        using value_type      = quiescence_buf_elem;
        using reference       = value_type&;
        using const_reference = const value_type&;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using iterator        = pointer;
        using const_iterator  = const_pointer;

    private:
        static_assert(std::is_pod<value_type>{}, "only works with POD types");
        static_assert(std::is_same<value_type, typename allocator_type::value_type>{}, "");
        static_assert(is_power_of_two(ReclaimLimit), "");
        static_assert(ReclaimLimit > 1, "");
        static constexpr bool has_noexcept_alloc
            = noexcept(std::declval<Alloc&>().allocate(std::declval<uword>()));

        uword    epoch_size_;
        iterator epoch_begin_;
        iterator ring_begin;
        uword    ring_size;

        pointer buffer;
        uword   capacity_;

        using alloc_traits = std::allocator_traits<allocator_type>;
        static_assert(std::is_pointer<typename alloc_traits::pointer>{},
                      "sorry, lstm currently only supports allocators that return "
                      "raw pointers");

        inline allocator_type& alloc() noexcept { return *this; }

        LSTM_NOINLINE void reserve_more() noexcept(has_noexcept_alloc)
        {
            LSTM_ASSERT(is_power_of_two(capacity_));
            LSTM_ASSERT(capacity() == size() + 1);
            LSTM_ASSERT((capacity() << 1) > size()); // zomg big transaction
            const pointer new_buffer = alloc().allocate(capacity() << 1);
            LSTM_ASSERT(new_buffer);

            const uword amount_to_copy = static_cast<uword>((buffer + capacity_) - ring_begin);
            std::memcpy(new_buffer, ring_begin, sizeof(value_type) * amount_to_copy);
            std::memcpy(new_buffer + amount_to_copy,
                        buffer,
                        sizeof(value_type) * (ring_size - amount_to_copy));

            alloc().deallocate(buffer, capacity());

            capacity_ <<= 1;
            ring_begin   = new_buffer;
            buffer       = new_buffer;
            epoch_begin_ = new_buffer + ring_size - epoch_size_;

            LSTM_ASSERT(is_power_of_two(capacity_));
        }

        void initialize_header(const iterator iter, const uword prev_size) noexcept
        {
            ::new (&iter->header) quiescence_header;
            iter->header.prev_size = prev_size;
            iter->header.size      = 1;
        }

        iterator bump_iterator(const iterator iter) const noexcept
        {
            return buffer + (((iter + 1) - buffer) & (capacity() - 1));
        }

        iterator bump_iterator(const iterator iter, const int amount) const noexcept
        {
            return buffer + (((iter + amount) - buffer) & (capacity() - 1));
        }

    public:
        quiescence_buffer(const allocator_type& alloc = {}) noexcept(has_noexcept_alloc)
            : allocator_type(alloc)
            , epoch_begin_(this->alloc().allocate(ReclaimLimit * 2))
            , ring_begin(epoch_begin_)
            , ring_size(1)
            , buffer(epoch_begin_)
            , capacity_(ReclaimLimit * 2)
        {
            initialize_header(epoch_begin_, 0);
            epoch_size_ = 1;
        }

        quiescence_buffer(const quiescence_buffer&) = delete;
        quiescence_buffer& operator=(const quiescence_buffer&) = delete;

        ~quiescence_buffer() noexcept
        {
            LSTM_ASSERT(empty());
            alloc().deallocate(buffer, capacity());
        }

        bool  empty() const noexcept { return ring_size == 1; }
        uword size() const noexcept { return ring_size - 1; }
        uword capacity() const noexcept { return capacity_; }
        bool  allocates_on_next_push() const noexcept { return ring_size + 1 == capacity(); }
        bool  working_epoch_empty() const noexcept { return epoch_size_ == 1; }
        void  clear_working_epoch() noexcept
        {
            LSTM_ASSERT(ring_size >= epoch_size_);
            ring_size   = ring_size - epoch_size_ + 1;
            epoch_size_ = 1;
        }

        template<typename... Us>
        void emplace_back(Us&&... us) noexcept(has_noexcept_alloc)
        {
            const iterator cur = bump_iterator(epoch_begin_, epoch_size_);
            ::new (&cur->callback) gp_callback((Us &&) us...);
            ++epoch_size_;
            ++ring_size;
            if (LSTM_UNLIKELY(ring_size == capacity()))
                reserve_more();
        }

        template<typename... Us>
        void unchecked_emplace_back(Us&&... us) noexcept
        {
            // if you hit this you probly forgot to check allocates_on_next_push
            LSTM_ASSERT(!allocates_on_next_push());

            const iterator cur = bump_iterator(epoch_begin_, epoch_size_);
            ::new (&cur->callback) gp_callback((Us &&) us...);
            ++epoch_size_;
            ++ring_size;
        }

        void shrink_to_fit() noexcept(has_noexcept_alloc) { throw "TODO"; }

        bool finalize_epoch(const epoch_t epoch) noexcept(has_noexcept_alloc)
        {
            if (working_epoch_empty())
                return false;

            LSTM_ASSERT(epoch_size_ > 1);

            epoch_begin_->header.epoch = epoch;
            epoch_begin_->header.size  = epoch_size_;

            const uword prev_size = epoch_size_;
            epoch_begin_          = bump_iterator(epoch_begin_, epoch_size_);
            initialize_header(epoch_begin_, prev_size);
            epoch_size_ = 1;
            ++ring_size;
            if (LSTM_UNLIKELY(ring_size == capacity()))
                reserve_more();

            return ring_size >= ReclaimLimit;
        }

        void do_first_epoch_callbacks() noexcept
        {
            LSTM_ASSERT(working_epoch_empty());
            LSTM_ASSERT(ring_size > 1);

            iterator    iter     = ring_begin;
            const uword cur_size = iter->header.size;
            LSTM_ASSERT(cur_size > 1);

            const iterator end = bump_iterator(iter, cur_size);
            for (iter = bump_iterator(iter); iter != end; iter = bump_iterator(iter))
                iter->callback();
            ring_begin = iter;

            LSTM_ASSERT(ring_size > cur_size);
            ring_size -= cur_size;

            LSTM_ASSERT(ring_begin->header.prev_size == cur_size);
        }

        epoch_t back_epoch() const noexcept
        {
            LSTM_ASSERT(!empty());
            const const_iterator iter
                = bump_iterator(epoch_begin_, -epoch_begin_->header.prev_size);
            return iter->header.epoch;
        }

        epoch_t front_epoch() const noexcept
        {
            LSTM_ASSERT(!empty());
            return ring_begin->header.epoch;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_QUIESCENCE_BUFFER_HPP */