#ifndef LSTM_DETAIL_QUIESCENCE_BUFFER_HPP
#define LSTM_DETAIL_QUIESCENCE_BUFFER_HPP

#include <lstm/detail/gp_callback.hpp>
#include <lstm/detail/pod_mallocator.hpp>

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

    // TODO: needs some optimization - probly store offsets not pointers
    // TODO: investigate merging small epoch chunks together (taking the max epoch) to reduce
    // memory overhead from quiescence_headers
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
        static_assert(ReclaimLimit > 2, "");
        static constexpr bool has_noexcept_alloc
            = noexcept(std::declval<Alloc&>().allocate(std::declval<uword>()));

        uword write_pos;
        uword epoch_begin;
        uword ring_begin;

        pointer buffer;
        uword   last_valid_index;

        using alloc_traits = std::allocator_traits<allocator_type>;
        static_assert(std::is_pointer<typename alloc_traits::pointer>{},
                      "sorry, lstm currently only supports allocators that return "
                      "raw pointers");

        inline allocator_type& alloc() noexcept { return *this; }

        uword wrap(const uword index) const noexcept { return index & (capacity() - 1); }

        uword size() const noexcept { return wrap(write_pos - ring_begin); }
        uword working_epoch_size() const noexcept { return wrap(write_pos - epoch_begin); }

        LSTM_NOINLINE void reserve_more() noexcept(has_noexcept_alloc)
        {
            LSTM_ASSERT(is_power_of_two(capacity()));
            LSTM_ASSERT((capacity() << 1) > capacity()); // zomg big transaction
            const pointer new_buffer = alloc().allocate(capacity() << 1);
            LSTM_ASSERT(new_buffer);

            const uword first_chunk_size = capacity() - ring_begin;
            std::memcpy(new_buffer, buffer + ring_begin, sizeof(value_type) * first_chunk_size);
            std::memcpy(new_buffer + first_chunk_size, buffer, sizeof(value_type) * ring_begin);

            alloc().deallocate(buffer, capacity());

            epoch_begin      = wrap(epoch_begin - ring_begin);
            write_pos        = capacity();
            last_valid_index = (capacity() << 1) - 1;
            ring_begin       = 0;
            buffer           = new_buffer;

            LSTM_ASSERT(is_power_of_two(capacity()));
        }

        void initialize_header(quiescence_buf_elem& elem, const uword prev_size) noexcept
        {
            ::new (&elem.header) quiescence_header;
            elem.header.prev_size = prev_size;
        }

    public:
        quiescence_buffer(const allocator_type& alloc = {}) noexcept(has_noexcept_alloc)
            : allocator_type(alloc)
            , write_pos(1)
            , epoch_begin(0)
            , ring_begin(0)
            , buffer(this->alloc().allocate(ReclaimLimit * 2))
            , last_valid_index(ReclaimLimit * 2 - 1)
        {
            initialize_header(*buffer, 0);
        }

        quiescence_buffer(const quiescence_buffer&) = delete;
        quiescence_buffer& operator=(const quiescence_buffer&) = delete;

        ~quiescence_buffer() noexcept
        {
            LSTM_ASSERT(empty());
            alloc().deallocate(buffer, capacity());
        }

        bool  empty() const noexcept { return size() == 1; }
        uword capacity() const noexcept { return last_valid_index + 1; }
        bool  allocates_on_next_push() const noexcept
        {
            return wrap(write_pos - ring_begin) == last_valid_index;
        }
        bool working_epoch_empty() const noexcept { return working_epoch_size() == 1; }
        void clear_working_epoch() noexcept
        {
            LSTM_ASSERT(size() >= working_epoch_size());
            write_pos = epoch_begin + 1;
        }

        template<typename... Us>
        void emplace_back(Us&&... us) noexcept(has_noexcept_alloc)
        {
            ::new (&buffer[wrap(write_pos++)].callback) gp_callback((Us &&) us...);
            if (LSTM_UNLIKELY(wrap(write_pos) == ring_begin))
                reserve_more();
        }

        template<typename... Us>
        void unchecked_emplace_back(Us&&... us) noexcept
        {
            // if you hit this you probly forgot to check allocates_on_next_push
            LSTM_ASSERT(!allocates_on_next_push());

            ::new (&buffer[wrap(write_pos++)].callback) gp_callback((Us &&) us...);
        }

        void shrink_to_fit() noexcept(has_noexcept_alloc) { throw "TODO"; }

        // TODO: make this smaller
        bool finalize_epoch(const epoch_t epoch) noexcept(has_noexcept_alloc)
        {
            if (working_epoch_empty())
                return false;

            LSTM_ASSERT(working_epoch_size() > 1);

            buffer[epoch_begin].header.epoch = epoch;
            buffer[epoch_begin].header.size  = working_epoch_size();

            const uword prev_size = working_epoch_size();
            epoch_begin           = wrap(write_pos);
            initialize_header(buffer[epoch_begin], prev_size);
            ++write_pos;
            if (LSTM_UNLIKELY(wrap(write_pos) == ring_begin))
                reserve_more();

            return size() >= ReclaimLimit;
        }

        void do_first_epoch_callbacks() noexcept
        {
            LSTM_ASSERT(working_epoch_empty());
            LSTM_ASSERT(size() > 1);
            LSTM_ASSERT(buffer[ring_begin].header.size > 1);
            LSTM_ASSERT(size() > buffer[ring_begin].header.size);

            uword       cur_idx  = ring_begin;
            const uword cur_size = buffer[cur_idx].header.size;

            const uword end = cur_idx + cur_size;
            for (++cur_idx; cur_idx != end; ++cur_idx)
                buffer[wrap(cur_idx)].callback();
            ring_begin = wrap(cur_idx);

            LSTM_ASSERT(buffer[ring_begin].header.prev_size == cur_size);
        }

        epoch_t back_epoch() const noexcept
        {
            LSTM_ASSERT(!empty());
            const uword idx = wrap(epoch_begin - buffer[epoch_begin].header.prev_size);
            return buffer[idx].header.epoch;
        }

        epoch_t front_epoch() const noexcept
        {
            LSTM_ASSERT(!empty());
            return buffer[ring_begin].header.epoch;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_QUIESCENCE_BUFFER_HPP */