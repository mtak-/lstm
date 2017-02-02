#ifndef LSTM_CONTAINERS_RBTREE_HPP
#define LSTM_CONTAINERS_RBTREE_HPP

// lots of copy paste from wikipedia. probly lots of cycles to be saved

#include <lstm/lstm.hpp>
#include <iostream>

LSTM_DETAIL_BEGIN
    enum class color : bool
    {
        red,
        black,
    };

    template<typename Alloc, typename T>
    using rebind_to = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;

    template<typename Key, typename Value, typename Alloc>
    struct rb_node_
    {
        lstm::var<Key, rebind_to<Alloc, Key>>     key;
        lstm::var<Value, rebind_to<Alloc, Value>> value;
        lstm::var<void*, rebind_to<Alloc, void*>> parent_;
        lstm::var<void*, rebind_to<Alloc, void*>> left_;
        lstm::var<void*, rebind_to<Alloc, void*>> right_;
        lstm::var<color, rebind_to<Alloc, color>> color_;

        template<typename T,
                 typename U,
                 LSTM_REQUIRES_(std::is_constructible<Key, T&&>{}
                                && std::is_constructible<Value, U&&>{})>
        rb_node_(T&& t, U&& u) noexcept(std::is_nothrow_constructible<Key, T&&>{}
                                        && std::is_nothrow_constructible<Value, U&&>{})
            : key((T &&) t)
            , value((U &&) u)
            , parent_(nullptr)
            , left_(nullptr)
            , right_(nullptr)
            , color_(color::red)
        {
        }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename Key,
             typename Value,
             typename Alloc   = std::allocator<std::pair<Key, Value>>,
             typename Compare = std::less<>>
    struct rbtree : private Compare,
                    private detail::rebind_to<Alloc, detail::rb_node_<Key, Value, Alloc>>
    {
    private:
        using node_t  = detail::rb_node_<Key, Value, Alloc>;
        using alloc_t = detail::rebind_to<Alloc, node_t>;
        lstm::var<void*, detail::rebind_to<Alloc, void*>> root_;

        const Compare& comparer() const noexcept { return *this; }

        alloc_t&       alloc() noexcept { return *this; }
        const alloc_t& alloc() const noexcept { return *this; }

        template<typename T, typename U>
        bool compare(T&& t, U&& u) const noexcept(noexcept(bool(comparer()((T &&) t, (U &&) u))))
        {
            return bool(comparer()((T &&) t, (U &&) u));
        }

        node_t* grandparent(transaction& tx, node_t* n)
        {
            if (n == nullptr)
                return nullptr;

            node_t* parent = (node_t*)tx.read(n->parent_);
            if (parent == nullptr)
                return nullptr;

            return (node_t*)tx.read(parent->parent_);
        }

        node_t* uncle(transaction& tx, node_t* n)
        {
            node_t* g = grandparent(tx, n);
            if (g == nullptr)
                return nullptr;

            node_t* left = (node_t*)tx.read(g->left_);
            if (tx.read(n->parent_) == left)
                return (node_t*)tx.read(g->right_);
            else
                return left;
        }

        void insert_case1(transaction& tx, node_t* n)
        {
            if (tx.read(n->parent_) == nullptr)
                tx.write(n->color_, detail::color::black);
            else
                insert_case2(tx, n);
        }

        void insert_case2(transaction& tx, node_t* n)
        {
            if (tx.read(((node_t*)tx.read(n->parent_))->color_) == detail::color::black)
                return;
            else
                insert_case3(tx, n);
        }

        void insert_case3(transaction& tx, node_t* n)
        {
            node_t* u = uncle(tx, n);

            if (u != nullptr && tx.read(u->color_) == detail::color::red) {
                tx.write(((node_t*)tx.read(n->parent_))->color_, detail::color::black);
                tx.write(u->color_, detail::color::black);
                node_t* g = grandparent(tx, n);
                tx.write(g->color_, detail::color::red);
                insert_case1(tx, g);
            } else {
                insert_case4(tx, n);
            }
        }

        void insert_case4(transaction& tx, node_t* n)
        {
            node_t* g = grandparent(tx, n);

            if (n == tx.read(((node_t*)tx.read(n->parent_))->right_)
                && tx.read(n->parent_) == tx.read(g->left_)) {
                // rotate_left(n->parent);

                node_t* saved_p      = (node_t*)tx.read(g->left_);
                node_t* saved_left_n = (node_t*)tx.read(n->left_);

                tx.write(g->left_, n);
                tx.write(n->parent_, g);

                tx.write(n->left_, saved_p);
                tx.write(saved_p->parent_, n);

                tx.write(saved_p->right_, saved_left_n);
                if (saved_left_n)
                    tx.write(saved_left_n->parent_, saved_p);

                n = saved_p;
            } else if (n == tx.read(((node_t*)tx.read(n->parent_))->left_)
                       && tx.read(n->parent_) == tx.read(g->right_)) {
                // rotate_right(n->parent);

                node_t* saved_p       = (node_t*)tx.read(g->right_);
                node_t* saved_right_n = (node_t*)tx.read(n->right_);

                tx.write(g->right_, n);
                tx.write(n->parent_, g);

                tx.write(n->right_, saved_p);
                tx.write(saved_p->parent_, n);

                tx.write(saved_p->left_, saved_right_n);
                if (saved_right_n)
                    tx.write(saved_right_n->parent_, saved_p);

                n = saved_p;
            }
            insert_case5(tx, n);
        }

        void insert_case5(transaction& tx, node_t* n)
        {
            node_t* g = grandparent(tx, n);

            tx.write(((node_t*)tx.read(n->parent_))->color_, detail::color::black);
            tx.write(g->color_, detail::color::red);
            if (n == tx.read(((node_t*)tx.read(n->parent_))->left_))
                rotate_right(tx, g);
            else {
                rotate_left(tx, g);
            }
        }

        void rotate_left(transaction& tx, node_t* n)
        {
            node_t* p        = (node_t*)tx.read(n->right_);
            node_t* p_left   = (node_t*)tx.read(p->left_);
            node_t* n_parent = (node_t*)tx.read(n->parent_);
            bool    left     = n_parent && tx.read(n_parent->left_) == n;
            tx.write(n->right_, p_left);

            if (p_left)
                tx.write(p_left->parent_, n);

            tx.write(p->left_, n);
            tx.write(n->parent_, p);

            tx.write(p->parent_, n_parent);
            if (!n_parent)
                tx.write(root_, p);
            else if (left)
                tx.write(n_parent->left_, p);
            else
                tx.write(n_parent->right_, p);
        }

        void rotate_right(transaction& tx, node_t* n)
        {
            node_t* p        = (node_t*)tx.read(n->left_);
            node_t* p_right  = (node_t*)tx.read(p->right_);
            node_t* n_parent = (node_t*)tx.read(n->parent_);
            bool    left     = n_parent && tx.read(n_parent->left_) == n;
            tx.write(n->left_, p_right);

            if (p_right)
                tx.write(p_right->parent_, n);

            tx.write(p->right_, n);
            tx.write(n->parent_, p);

            tx.write(p->parent_, n_parent);
            if (!n_parent)
                tx.write(root_, p);
            else if (left)
                tx.write(n_parent->left_, p);
            else
                tx.write(n_parent->right_, p);
        }

        void push_impl(transaction& tx, node_t* new_node)
        {
            node_t* prev_parent = nullptr;
            node_t* parent;

            auto* cur = &root_;

            while ((parent = (node_t*)tx.read(*cur))) {
                cur = compare(new_node->key.unsafe_read(), tx.read(parent->key)) ? &parent->left_
                                                                                 : &parent->right_;
                prev_parent = parent;
            }

            tx.write(new_node->parent_, prev_parent);
            tx.write(*cur, new_node);
            insert_case1(tx, new_node);
        }

        std::pair<std::size_t, std::size_t> minmax_height(transaction& tx, node_t* node) const
        {
            if (!node)
                return {0, 0};

            auto left     = (node_t*)tx.read(node->left_);
            auto right    = (node_t*)tx.read(node->right_);
            auto height_l = minmax_height(tx, left);
            auto height_r = minmax_height(tx, right);

            assert(!left || tx.read(left->parent_) == node);
            assert(!right || tx.read(right->parent_) == node);

            return {std::min(height_l.first, height_r.first) + 1,
                    std::max(height_l.second, height_r.second) + 1};
        }

    public:
        rbtree() noexcept
            : root_{nullptr}
        {
        }

        // TODO: these
        rbtree(const rbtree&) = delete;
        rbtree& operator=(const rbtree&) = delete;
        ~rbtree() {}

        // todo concept check
        template<typename U>
        node_t* find(transaction& tx, const U& u) const
        {
            node_t* cur = (node_t*)tx.read(root_);
            while (cur) {
                if (compare(u, tx.read(cur->key_value.first)))
                    cur = (node_t*)tx.read(cur->left_);
                else if (compare(tx.read(cur->key_value.first), u))
                    cur = (node_t*)tx.read(cur->right_);
                else
                    break;
            }
            return cur;
        }

        template<typename... Us, LSTM_REQUIRES_(std::is_constructible<node_t, Us&&...>{})>
        void emplace(transaction& tx, Us&&... us)
        {
            push_impl(tx, lstm::allocate_construct(alloc(), (Us &&) us...));
        }

        void clear() { throw "todo"; }

        bool verify(transaction& tx) const
        {
            const auto heights = minmax_height(tx, (node_t*)tx.read(root_));
            return heights.first * 2 >= heights.second;
        }

        //        const node_t* begin(transaction& tx) const
        //        {
        //            const node_t* cur = (const node_t*)tx.read(root_);
        //            while (cur) {
        //                const node_t* next = (const node_t*)tx.read(cur->left_);
        //                if (!next)
        //                    return cur;
        //                cur = next;
        //            }
        //            return cur;
        //        }
        //
        //        const node_t* succ(transaction& tx, const node_t* n) const
        //        {
        //            if (const node_t* p = (const node_t*)tx.read(n->right_)) {
        //                do {
        //                    const node_t* next = (const node_t*)tx.read(p->left_);
        //                    if (!next)
        //                        return p;
        //                    p = next;
        //                } while (true);
        //            } else {
        //				const node_t* parent = (const node_t*)tx.read(n->parent_);
        //				if (parent && n != (const node_t*)tx.read(parent->left_))
        //					return (const node_t*)tx.read(parent->parent_);
        //				else
        //					return parent;
        //            }
        //        }

        void print_impl(transaction& tx, const node_t* n) const
        {
            if (!n)
                return;

            if (auto left = (const node_t*)tx.read(n->left_)) {
                std::cout << "left: {";
                print_impl(tx, left);
                std::cout << "}";
            }
            std::cout << '[' << tx.read(n->key) << ", " << tx.read(n->value) << "], "
                      << static_cast<bool>(tx.read(n->color_)) << std::flush;

            if (auto right = (const node_t*)tx.read(n->right_)) {
                std::cout << "right: {";
                print_impl(tx, right);
                std::cout << "}";
            }
        }

        void print(transaction& tx) const { print_impl(tx, (const node_t*)tx.read(root_)); }
    };
LSTM_END

#endif /* LSTM_CONTAINERS_RBTREE_HPP */
