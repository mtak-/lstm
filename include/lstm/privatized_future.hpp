#ifndef LSTM_PRIVATIZED_FUTURE_HPP
#define LSTM_PRIVATIZED_FUTURE_HPP

#include <lstm/thread_data.hpp>

#include <exception>

LSTM_DETAIL_BEGIN
    // TODO: make this type smaller
    template<typename T>
    struct privatized_future_data
    {
    private:
        union data_t
        {
            T                  t;
            std::exception_ptr exc;
        };

        uninitialized<data_t> data;
        thread_data*          tls_td;
        std::int8_t           reclaim_buffer_idx;
        bool                  has_exception_;
        bool                  is_ready_;
        bool                  abandoned_;

        data_t& underlying() noexcept { return reinterpret_cast<data_t&>(data); }

        bool has_exception() const noexcept
        {
            LSTM_ASSERT(is_ready());
            return has_exception_;
        }

        void set_has_exception() noexcept
        {
            LSTM_ASSERT(!has_exception());
            has_exception_ = true;
        }

        void set_is_ready() noexcept
        {
            LSTM_ASSERT(!is_ready_);
            is_ready_ = true;
        }

        privatized_future_data() noexcept = default;

    public:
        privatized_future_data(thread_data* in_tls_td, std::int8_t in_reclaim_buffer_idx) noexcept
            : tls_td(in_tls_td)
            , reclaim_buffer_idx(in_reclaim_buffer_idx)
            , has_exception_(false)
            , is_ready_(false)
            , abandoned_(false)
        {
        }

        privatized_future_data(const privatized_future_data&) = delete;
        privatized_future_data& operator=(const privatized_future_data&) = delete;

        // if privatized_future_data has been abandoned, gp_callback must still free it
        // however, gp_callback will _never_ destruct privatized_future_data
        ~privatized_future_data() noexcept
        {
            if (is_ready()) {
                if (has_exception())
                    underlying().exc.~exception_ptr();
                else
                    underlying().t.~T();
            }
        }

        void wait() const noexcept
        {
            if (!is_ready())
                tls_td->reclaim_at(reclaim_buffer_idx);
        }

        T& get()
        {
            wait();

            LSTM_ASSERT(is_ready());

            if (has_exception())
                std::rethrow_exception(std::move(underlying().exc));
            return underlying().t;
        }

        // called from privatized_future
        bool is_ready() const noexcept { return is_ready_; }

        void abandon() noexcept
        {
            LSTM_ASSERT(!is_ready());
            LSTM_ASSERT(!abandoned());
            abandoned_ = true;
        }

        // called from gp_callback
        bool abandoned() const noexcept { return abandoned_; }
        void set_exception(std::exception_ptr in_exc) noexcept(
            std::is_nothrow_move_constructible<std::exception_ptr>{})
        {
            LSTM_ASSERT(!abandoned());
            LSTM_ASSERT(!is_ready());
            LSTM_ASSERT(!has_exception());

            underlying().exc = std::move(in_exc);
            set_has_exception();
            set_is_ready();

            LSTM_ASSERT(is_ready());
        }

        void set_value(const T& in_t) noexcept(std::is_nothrow_copy_constructible<T>{})
        {
            LSTM_ASSERT(!abandoned());
            LSTM_ASSERT(!is_ready());
            LSTM_ASSERT(!has_exception());

            underlying().t = in_t;
            set_is_ready();

            LSTM_ASSERT(is_ready());
        }

        void set_value(T&& in_t) noexcept(std::is_nothrow_move_constructible<T>{})
        {
            LSTM_ASSERT(!abandoned());
            LSTM_ASSERT(!is_ready());
            LSTM_ASSERT(!has_exception());

            underlying().t = std::move(in_t);
            set_is_ready();

            LSTM_ASSERT(is_ready());
        }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename T>
    struct privatized_future
    {
    private:
        using data_t = detail::privatized_future_data<T>;
        std::unique_ptr<data_t> data;

        friend detail::gp_callback;

        explicit privatized_future(data_t* in_data) noexcept
            : data(in_data)
        {
            LSTM_ASSERT(valid());
            LSTM_ASSERT(!is_ready());
        }

    public:
        privatized_future() noexcept
            : data{nullptr}
        {
        }

        privatized_future(privatized_future&&) noexcept = default;
        privatized_future& operator=(privatized_future&&) noexcept = default;

        ~privatized_future()
        {
            if (valid() && !is_ready())
                data.release()->abandon();
        }

        bool valid() const noexcept { return static_cast<bool>(data); }
        bool is_ready() const noexcept
        {
            LSTM_ASSERT(valid());
            return data->is_ready();
        }

        T& get() &
        {
            LSTM_ASSERT(valid());

            return data->get();
        }

        const T& get() const &
        {
            LSTM_ASSERT(valid());

            return data->get();
        }

        T&& get() &&
        {
            LSTM_ASSERT(valid());

            return std::move(data->get());
        }

        const T&& get() const &&
        {
            LSTM_ASSERT(valid());

            return std::move(data->get());
        }

        void wait() const noexcept
        {
            LSTM_ASSERT(valid());

            data->wait();
        }
    };
LSTM_END

#endif /* LSTM_PRIVATIZED_FUTURE_HPP */