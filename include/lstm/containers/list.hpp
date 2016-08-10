#ifndef LSTM_CONTAINERS_LIST_HPP
#define LSTM_CONTAINERS_LIST_HPP

#include <lstm/lstm.hpp>

LSTM_BEGIN
    template<typename T>
    struct list {
    private:
        struct node {
            T t;
            node* next;
        };
        lstm::var<node*> head{nullptr};
        lstm::var<word> _size{0};
        
    public:
        constexpr list() = default;
        
        template<typename... Us>
        void emplace_front(Us&&... us) noexcept(std::is_nothrow_constructible<T, Us&&...>{}) {
            auto new_head = new node{{(Us&&)us...}};
            lstm::atomic([&](auto& tx) {
                new_head->next = tx.load(head);
                tx.store(_size, tx.load(_size) + 1);
                tx.store(head, new_head);
            });
        }
        
        word size() const noexcept
        { return lstm::atomic([&](auto& tx) { return tx.load(_size); }); }
    };
LSTM_END

#endif /* LSTM_CONTAINERS_LIST_HPP */