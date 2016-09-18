#ifndef LSTM_CONTAINERS_LIST_HPP
#define LSTM_CONTAINERS_LIST_HPP

#include <lstm/lstm.hpp>

#error "TODO: lstm/containers/list.hpp is not a working implementation"

LSTM_BEGIN
    namespace detail {
        template<typename T>
        struct _list_node {
            lstm::var<T> value;
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
        static node* next(node& n, Transaction& tx)
        { return reinterpret_cast<node*>(tx.load(n._next)); }
        
        template<typename Transaction>
        static const node* next(const node& n, Transaction& tx)
        { return reinterpret_cast<const node*>(tx.load(n._next)); }
        
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
        
        void clear() {
            std::vector<node*> to_delete;
            lstm::atomic([&](auto& tx) {
                to_delete.clear();
                
                auto node = tx.load(head);
                
                if (node) {
                    while (node) {
                        to_delete.push_back(node);
                        auto next_node = next(*node, tx);
                        tx.store(node->_next, nullptr);
                        node = next_node;
                    }
                    tx.store(_size, 0);
                    tx.store(head, nullptr);
                }
            });
            for (auto node_ptr : to_delete)
                delete node_ptr;
        }
        
        template<typename... Us>
        void emplace_front(Us&&... us) noexcept(std::is_nothrow_constructible<T, Us&&...>{}) {
            auto new_head = (node*)malloc(sizeof(node));
            new (&new_head->value) lstm::var<T>{(Us&&)us...};
            new (&new_head->_next) lstm::var<void*>(nullptr);
            
            lstm::atomic([&](auto& tx) {
                auto _head = tx.load(head);
                unsafe_next(*new_head) = _head;
                tx.store(_size, tx.load(_size) + 1);
                tx.store(head, new_head);
            });
        }
        
        word size() const noexcept
        { return lstm::atomic([&](auto& tx) { return tx.load(_size); }); }
    };
LSTM_END

#endif /* LSTM_CONTAINERS_LIST_HPP */