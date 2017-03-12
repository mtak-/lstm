#ifndef LSTM_CONTAINERS_LIST_HPP
#define LSTM_CONTAINERS_LIST_HPP

#include <lstm/lstm.hpp>

LSTM_DETAIL_BEGIN
    template<typename Alloc, typename T>
    using rebind_to = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;

    template<typename T, typename Alloc>
    struct list_node_
    {
        lstm::var<T, rebind_to<Alloc, T>>         value;
        lstm::var<void*, rebind_to<Alloc, void*>> prev_;
        lstm::var<void*, rebind_to<Alloc, void*>> next_;

        template<typename... Us, LSTM_REQUIRES_(sizeof...(Us) != 1)>
        list_node_(Us&&... us) noexcept(std::is_nothrow_constructible<T, Us&&...>{})
            : value((Us &&) us...)
        {
        }

        template<typename U, LSTM_REQUIRES_(!std::is_same<uncvref<U>, list_node_<T, Alloc>>{})>
        list_node_(U&& u) noexcept(std::is_nothrow_constructible<T, U&&>{})
            : value((U &&) u)
        {
        }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename T, typename Alloc_ = std::allocator<T>>
    struct list : private detail::rebind_to<Alloc_, detail::list_node_<T, Alloc_>>
    {
    private:
        using node_t       = detail::list_node_<T, Alloc_>;
        using Alloc        = detail::rebind_to<Alloc_, node_t>;
        using alloc_traits = std::allocator_traits<Alloc>;

        lstm::var<node_t*, detail::rebind_to<Alloc, node_t*>> head{nullptr};
        lstm::var<word, detail::rebind_to<Alloc, word>>       size_{0};

        Alloc&       alloc() noexcept { return *this; }
        const Alloc& alloc() const noexcept { return *this; }

        template<typename Transaction>
        static node_t* prev(node_t& n, Transaction& tx)
        {
            return reinterpret_cast<node_t*>(n.prev_.get(tx));
        }

        template<typename Transaction>
        static const node_t* prev(const node_t& n, Transaction& tx)
        {
            return reinterpret_cast<const node_t*>(n.prev_.get(tx));
        }

        template<typename Transaction>
        static node_t* next(node_t& n, Transaction& tx)
        {
            return reinterpret_cast<node_t*>(n.next_.get(tx));
        }

        template<typename Transaction>
        static const node_t* next(const node_t& n, Transaction& tx)
        {
            return reinterpret_cast<const node_t*>(n.next_.get(tx));
        }

        static node_t* unsafe_prev(node_t& n) noexcept
        {
            return reinterpret_cast<node_t*>(n.prev_.unsafe_get());
        }

        static const node_t* unsafe_prev(const node_t& n) noexcept
        {
            return reinterpret_cast<const node_t*>(n.prev_.unsafe_get());
        }

        static node_t* unsafe_next(node_t& n) noexcept
        {
            return reinterpret_cast<node_t*>(n.next_.unsafe_get());
        }

        static const node_t* unsafe_next(const node_t& n) noexcept
        {
            return reinterpret_cast<const node_t*>(n.next_.unsafe_get());
        }

        static void destroy_deallocate_sublist(Alloc alloc, node_t* node)
        {
            while (node) {
                auto next_ = unsafe_next(*node);
                alloc_traits::destroy(alloc, node);
                alloc_traits::deallocate(alloc, node, 1);
                node = next_;
            }
        }

    public:
        constexpr list() noexcept(std::is_nothrow_default_constructible<Alloc>{}) = default;
        constexpr list(const Alloc_& alloc) noexcept(
            std::is_nothrow_constructible<Alloc, const Alloc_&>{})
            : Alloc(alloc)
        {
        }

        ~list() { destroy_deallocate_sublist(alloc(), head.unsafe_get()); }

        void clear()
        {
            thread_data& tls_td = tls_thread_data();
            lstm::atomic(tls_td, [&](const transaction tx) {
                size_.set(tx, 0);
                auto root = head.get(tx);
                head.set(tx, nullptr);
                if (root) {
                    tls_td.sometime_after([ alloc = alloc(), root ]() noexcept {
                        destroy_deallocate_sublist(alloc, root);
                    });
                }
            });
        }

        template<typename... Us>
        void emplace_front(Us&&... us)
        {
            thread_data& tls_td   = tls_thread_data();
            node_t*      new_head = lstm::allocate_construct(tls_td, alloc(), (Us &&) us...);
            lstm::atomic(tls_td, [&](const transaction tx) {
                auto head_ = head.get(tx);
                new_head->next_.unsafe_set(head_);

                if (head_)
                    head_->prev_.set(tx, (void*)new_head);
                size_.set(tx, size_.get(tx) + 1);
                head.set(tx, new_head);
            });
        }

        word size() const
        {
            return lstm::atomic([&](const transaction tx) { return size_.get(tx); });
        }
    };
LSTM_END

#endif /* LSTM_CONTAINERS_LIST_HPP */
