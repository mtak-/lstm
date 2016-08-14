#ifndef LSTM_CONTAINERS_LIST_HPP
#define LSTM_CONTAINERS_LIST_HPP

#include <lstm/lstm.hpp>

LSTM_BEGIN
    namespace detail {
        template<typename T>
        struct _list_node {
            lstm::var<T> value;
            lstm::var<void*> _prev;
            lstm::var<void*> _next;
        };
    }
    
    template<typename T>
    struct list {
    private:
        using node = detail::_list_node<T>;
        lstm::var<node*> head{nullptr};
        lstm::var<word> _size{0};
        
        template<typename Transaction>
        static node* prev(node& n, Transaction& tx)
        { return reinterpret_cast<node*>(tx.load(n._prev)); }
        
        template<typename Transaction>
        static const node* prev(const node& n, Transaction& tx)
        { return reinterpret_cast<const node*>(tx.load(n._prev)); }
        
        template<typename Transaction>
        static node* next(node& n, Transaction& tx)
        { return reinterpret_cast<node*>(tx.load(n._next)); }
        
        template<typename Transaction>
        static const node* next(const node& n, Transaction& tx)
        { return reinterpret_cast<const node*>(tx.load(n._next)); }
        
        static node*& unsafe_prev(node& n)
        { return reinterpret_cast<node*&>(n._prev.unsafe()); }
        
        static const node*& unsafe_prev(const node& n)
        { return reinterpret_cast<const node*&>(n._prev.unsafe()); }
        
        static node*& unsafe_next(node& n)
        { return reinterpret_cast<node*&>(n._next.unsafe()); }
        
        static const node*& unsafe_next(const node& n)
        { return reinterpret_cast<const node*&>(n._next.unsafe()); }
        
    public:
        constexpr list() = default;
        
        ~list() {
            auto node = head.unsafe();
            _size.unsafe() = 0;
            while (node) {
                auto to_delete = node;
                node = unsafe_next(*node);
                delete to_delete;
            }
        }
        
        void clear() noexcept {
            lstm::atomic([&](auto& tx) {
                auto node = tx.load(head);
                tx.store(_size, 0);
                while (node) {
                    auto to_delete = node;
                    node = next(*node, tx);
                    delete to_delete;
                }
            });
        }
        
        template<typename... Us>
        void emplace_front(Us&&... us) noexcept(std::is_nothrow_constructible<T, Us&&...>{}) {
            auto new_head = new node{{(Us&&)us...}};
            lstm::atomic([&](auto& tx) {
                auto _head = tx.load(head);
                unsafe_next(*new_head) = _head;
                if (_head)
                    tx.store(_head->_prev, new_head);
                tx.store(_size, tx.load(_size) + 1);
                tx.store(head, new_head);
            });
        }
        
        word size() const noexcept
        { return lstm::atomic([&](auto& tx) { return tx.load(_size); }); }
    };
LSTM_END

#endif /* LSTM_CONTAINERS_LIST_HPP */