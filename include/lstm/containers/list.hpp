#ifndef LSTM_CONTAINERS_LIST_HPP
#define LSTM_CONTAINERS_LIST_HPP

#include <lstm/lstm.hpp>

LSTM_BEGIN
    namespace detail {
        template<typename Alloc, typename T>
        using rebind_to = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
        
        template<typename T, typename Alloc>
        struct _list_node {
            lstm::var<T, rebind_to<Alloc, T>> value;
            lstm::var<void*, rebind_to<Alloc, void*>> _prev;
            lstm::var<void*, rebind_to<Alloc, void*>> _next;
            
            template<typename... Us,
                LSTM_REQUIRES_(sizeof...(Us) != 1)>
            _list_node(Us&&... us) noexcept(std::is_nothrow_constructible<T, Us&&...>{})
                : value((Us&&)us...)
            {}
            
            template<typename U,
                LSTM_REQUIRES_(!std::is_same<uncvref<U>, _list_node<T, Alloc>>{})>
            _list_node(U&& u) noexcept(std::is_nothrow_constructible<T, U&&>{})
                : value((U&&)u)
            {}
        };
    }
    
    template<typename T, typename Alloc_ = std::allocator<T>>
    struct list
        : private detail::rebind_to<Alloc_, detail::_list_node<T, Alloc_>>
    {
    private:
        using node_t = detail::_list_node<T, Alloc_>;
        using Alloc = detail::rebind_to<Alloc_, node_t>;
        using alloc_traits = std::allocator_traits<Alloc>;
        
        lstm::var<node_t*, detail::rebind_to<Alloc, node_t*>> head{nullptr};
        lstm::var<word, detail::rebind_to<Alloc, word>> _size{0};
        
        Alloc& alloc() noexcept { return *this; }
        const Alloc& alloc() const noexcept { return *this; }
        
        template<typename Transaction>
        static node_t* prev(node_t& n, Transaction& tx)
        { return reinterpret_cast<node_t*>(tx.read(n._prev)); }
        
        template<typename Transaction>
        static const node_t* prev(const node_t& n, Transaction& tx)
        { return reinterpret_cast<const node_t*>(tx.read(n._prev)); }
        
        template<typename Transaction>
        static node_t* next(node_t& n, Transaction& tx)
        { return reinterpret_cast<node_t*>(tx.read(n._next)); }
        
        template<typename Transaction>
        static const node_t* next(const node_t& n, Transaction& tx)
        { return reinterpret_cast<const node_t*>(tx.read(n._next)); }
        
        static node_t* unsafe_prev(node_t& n) noexcept
        { return reinterpret_cast<node_t*>(n._prev.unsafe_read()); }
        
        static const node_t* unsafe_prev(const node_t& n) noexcept
        { return reinterpret_cast<const node_t*>(n._prev.unsafe_read()); }
        
        static node_t* unsafe_next(node_t& n) noexcept
        { return reinterpret_cast<node_t*>(n._next.unsafe_read()); }
        
        static const node_t* unsafe_next(const node_t& n) noexcept
        { return reinterpret_cast<const node_t*>(n._next.unsafe_read()); }
        
    public:
        constexpr list() noexcept(std::is_nothrow_default_constructible<Alloc>{}) = default;
        constexpr list(const Alloc_& alloc)
            noexcept(std::is_nothrow_constructible<Alloc, const Alloc_&>{})
            : Alloc(alloc)
        {}
        
        ~list() {
            auto node = head.unsafe_read();
            while (node) {
                auto next_ = unsafe_next(*node);
                alloc_traits::destroy(alloc(), node);
                alloc_traits::deallocate(alloc(), node, 1);
                node = next_;
            }
        }
        
        void clear() {
            thread_data& tls_td = tls_thread_data();
            auto* node = lstm::read_write([&](auto& tx) {
                tx.write(_size, 0);
                auto result = tx.read(head);
                tx.write(head, nullptr);
                return result;
            }, default_domain(), tls_td);
            if (node) {
                const gp_t clock = default_domain().get_clock();
                while (node) {
                    auto next_ = unsafe_next(*node);
                    lstm::destroy_deallocate(tls_td, alloc(), node);
                    node = next_;
                }
                if (!tls_td.in_critical_section()) {
                    tls_td.synchronize(clock);
                    tls_td.do_succ_callbacks();
                }
            }
        }
        
        template<typename... Us>
        void emplace_front(Us&&... us) {
            thread_data& tls_td = tls_thread_data();
            node_t* new_head = lstm::allocate_construct(tls_td, alloc(), (Us&&)us...);
            lstm::read_write([&](auto& tx) {
                auto _head = tx.read(head);
                new_head->_next.unsafe_write(_head);
                
                if (_head)
                    tx.write(_head->_prev, (void*)new_head);
                tx.write(_size, tx.read(_size) + 1);
                tx.write(head, new_head);
            }, default_domain(), tls_td);
        }
        
        word size() const {
            return lstm::read_write([&](auto& tx) { return tx.read(_size); });
        }
    };
LSTM_END

#endif /* LSTM_CONTAINERS_LIST_HPP */
