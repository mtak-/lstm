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
        
        lstm::var<node_t*> head{nullptr};
        lstm::var<word> _size{0};
        
        Alloc& alloc() noexcept { return *this; }
        const Alloc& alloc() const noexcept { return *this; }
        
        template<typename Transaction>
        static node_t* prev(node_t& n, Transaction& tx)
        { return reinterpret_cast<node_t*>(tx.load(n._prev)); }
        
        template<typename Transaction>
        static const node_t* prev(const node_t& n, Transaction& tx)
        { return reinterpret_cast<const node_t*>(tx.load(n._prev)); }
        
        template<typename Transaction>
        static node_t* next(node_t& n, Transaction& tx)
        { return reinterpret_cast<node_t*>(tx.load(n._next)); }
        
        template<typename Transaction>
        static const node_t* next(const node_t& n, Transaction& tx)
        { return reinterpret_cast<const node_t*>(tx.load(n._next)); }
        
        static node_t* unsafe_prev(node_t& n) noexcept
        { return reinterpret_cast<node_t*>(n._prev.unsafe_load()); }
        
        static const node_t* unsafe_prev(const node_t& n) noexcept
        { return reinterpret_cast<const node_t*>(n._prev.unsafe_load()); }
        
        static node_t* unsafe_next(node_t& n) noexcept
        { return reinterpret_cast<node_t*>(n._next.unsafe_load()); }
        
        static const node_t* unsafe_next(const node_t& n) noexcept
        { return reinterpret_cast<const node_t*>(n._next.unsafe_load()); }
        
    public:
        constexpr list() noexcept(std::is_nothrow_default_constructible<Alloc>{}) = default;
        constexpr list(const Alloc_& alloc)
            noexcept(std::is_nothrow_constructible<Alloc, const Alloc_&>{})
            : Alloc(alloc)
        {}
        
        ~list() {
            auto node = head.unsafe_load();
            while (node) {
                auto next_ = unsafe_next(*node);
                alloc_traits::destroy(alloc(), node);
                alloc_traits::deallocate(alloc(), node, 1);
                node = next_;
            }
        }
        
        void clear() noexcept {
            auto* node = lstm::atomic([&](auto& tx) {
                tx.store(_size, 0);
                auto result = tx.load(head);
                tx.store(head, nullptr);
                return result;
            }, nullptr, alloc());
            if (node)
                lstm::atomic([&](auto& tx) {
                    while (node) {
                        auto next_ = next(*node, tx);
                        tx.delete_(node, alloc());
                        node = next_;
                    }
                }, nullptr, alloc());
        }
        
        template<typename... Us>
        void emplace_front(Us&&... us) noexcept(std::is_nothrow_constructible<T, Us&&...>{}) {
            auto new_head = alloc_traits::allocate(alloc(), 1);
            new (new_head) node_t((Us&&)us...);
            lstm::atomic([&](auto& tx) {
                auto _head = tx.load(head);
                new_head->_next.unsafe_store(_head);
                
                // TODO: segfault here
                // the issue is that in the commit phase, everything should be checked in the
                // same ORDER in which it was accessed
                // e.g. if to access y, one must first access x
                // then, deleting x, might also delete y.
                // if x happens to be readonly, currently it will be validated after ALL writes
                // thus the segfault here
                if (_head)
                    tx.store(_head->_prev, (void*)new_head);
                tx.store(_size, tx.load(_size) + 1);
                tx.store(head, new_head);
            }, nullptr, alloc());
        }
        
        word size() const noexcept
        { return lstm::atomic([&](auto& tx) { return tx.load(_size); }, nullptr, alloc()); }
    };
LSTM_END

#endif /* LSTM_CONTAINERS_LIST_HPP */