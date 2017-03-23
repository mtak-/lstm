#ifndef LSTM_PRIVATIZED_FUTURE_HPP
#define LSTM_PRIVATIZED_FUTURE_HPP

#include <lstm/thread_data.hpp>

#include <exception>

LSTM_DETAIL_BEGIN
    namespace
    {
        static constexpr auto exception_bit = lock_bit;
    }

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
        gp_t                  sync_version;

        data_t& underlying() { return reinterpret_cast<data_t&>(data); }

        bool has_exception() const noexcept
        {
            LSTM_ASSERT(is_ready());
            return sync_version == exception_bit;
        }

        void set_has_exception() noexcept
        {
            LSTM_ASSERT(!has_exception());
            sync_version = exception_bit;
        }

    public:
        privatized_future_data() noexcept                     = default;
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
                tls_td->reclaim(sync_version);
        }

        T& get()
        {
            LSTM_ASSERT(!is_ready());

            wait();

            LSTM_ASSERT(is_ready());

            if (has_exception())
                std::rethrow_exception(std::move(underlying().exc));
            return underlying().t;
        }

        // called from privatized_future
        bool is_ready() const noexcept { return tls_td == nullptr; }
        void abandon() noexcept { tls_td = nullptr; }

        // called from gp_callback
        bool abandoned() const noexcept { return tls_td == nullptr; }
        void set_exception(std::exception_ptr in_exc) noexcept(
            std::is_nothrow_move_constructible<std::exception_ptr>{})
        {
            LSTM_ASSERT(!abandoned());
            LSTM_ASSERT(!is_ready());

            underlying().exc = std::move(in_exc);
            set_has_exception();
            tls_td = nullptr;

            LSTM_ASSERT(is_ready());
        }

        void set_value(const T& in_t) noexcept(std::is_nothrow_copy_constructible<T>{})
        {
            LSTM_ASSERT(!abandoned());
            LSTM_ASSERT(!is_ready());

            underlying().t = in_t;
            tls_td         = nullptr;

            LSTM_ASSERT(is_ready());
        }

        void set_value(T&& in_t) noexcept(std::is_nothrow_move_constructible<T>{})
        {
            LSTM_ASSERT(!abandoned());
            LSTM_ASSERT(!is_ready());

            underlying().t = std::move(in_t);
            tls_td         = nullptr;

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