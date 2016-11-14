#ifndef LSTM_DETAIL_TX_RESULT_BUFFER_HPP
#define LSTM_DETAIL_TX_RESULT_BUFFER_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    struct return_tx_result_buffer_fn;

    template<typename T, bool = std::is_reference<T>{}>
    struct tx_result_buffer {
    private:
        uninitialized<T> data;
        bool initialized;
        
        friend return_tx_result_buffer_fn;
        
    public:
        tx_result_buffer() noexcept : initialized{false} {}
        tx_result_buffer(const tx_result_buffer&) = delete;
        tx_result_buffer& operator=(const tx_result_buffer&) = delete;
        
        template<typename U>
        void emplace(U&& u) noexcept(std::is_nothrow_constructible<T, U&&>{}) {
            assert(!initialized);
            ::new (&data) T((U&&)u);
            initialized = true;
        }
        
        bool is_initialized() const noexcept { return initialized; }
        void reset() noexcept {
            assert(initialized);
            initialized = false;
            reinterpret_cast<T&>(data).~T();
        }
    };

    template<typename T>
    struct tx_result_buffer<T, true> {
    private:
        std::remove_reference_t<T>* data;
        bool initialized{false};
        
        friend return_tx_result_buffer_fn;
        
    public:
        tx_result_buffer() noexcept : initialized{false} {}
        tx_result_buffer(const tx_result_buffer&) = delete;
        tx_result_buffer& operator=(const tx_result_buffer&) = delete;
        
        void emplace(T t) noexcept {
            data = &t;
            initialized = true;
        }
        
        bool is_initialized() const noexcept { return initialized; }
        void reset() noexcept {
            assert(initialized);
            initialized = false;
        }
    };
    
    template<>
    struct tx_result_buffer<void, false> {
        friend return_tx_result_buffer_fn;
        
    public:
        tx_result_buffer() noexcept = default;
        tx_result_buffer(const tx_result_buffer&) = delete;
        tx_result_buffer& operator=(const tx_result_buffer&) = delete;
        
        bool is_initialized() const noexcept { return false; }
        void reset() noexcept {}
    };
    
    struct return_tx_result_buffer_fn {
        void operator()(tx_result_buffer<void, false>&) noexcept {}
        
        template<typename T>
        T& operator()(tx_result_buffer<T&, true>& buf) noexcept {
            assert(buf.initialized);
            return *buf.data;
        }
        
        template<typename T>
        T&& operator()(tx_result_buffer<T&&, true>& buf) noexcept {
            assert(buf.initialized);
            return std::move(*buf.data);
        }
        
        template<typename T>
        T operator()(tx_result_buffer<T, false>& buf)
            noexcept(std::is_nothrow_move_constructible<T>{})
        {
            assert(buf.initialized);
            return std::move(reinterpret_cast<T&>(buf.data));
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_TX_RESULT_BUFFER_HPP */
