#ifndef LSTM_CONTAINERS_RBTREE_HPP
#define LSTM_CONTAINERS_RBTREE_HPP

// lots of copy paste from wikipedia. probly lots of cycles to be saved

#include <lstm/lstm.hpp>

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
        rb_node_(const Alloc& alloc,
                 T&&          t,
                 U&&          u) noexcept(std::is_nothrow_constructible<Key, T&&>{}
                                 && std::is_nothrow_constructible<Value, U&&>{})
            : key(std::allocator_arg, alloc, (T &&) t)
            , value(std::allocator_arg, alloc, (U &&) u)
            , parent_(std::allocator_arg, alloc, nullptr)
            , left_(std::allocator_arg, alloc, nullptr)
            , right_(std::allocator_arg, alloc, nullptr)
            , color_(std::allocator_arg, alloc, color::red)
        {
        }
    };
    struct height_info
    {
        std::size_t min_height;
        std::size_t max_height;
        std::size_t black_height;
    };
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename Key,
             typename Value,
             typename Compare = std::less<>,
             typename Alloc   = std::allocator<std::pair<Key, Value>>>
    struct rbtree : private Compare,
                    private detail::rebind_to<Alloc, detail::rb_node_<Key, Value, Alloc>>
    {
    private:
        using node_t       = detail::rb_node_<Key, Value, Alloc>;
        using alloc_t      = detail::rebind_to<Alloc, node_t>;
        using alloc_traits = std::allocator_traits<alloc_t>;
        lstm::var<void*, detail::rebind_to<Alloc, void*>> root_;

        template<typename T, typename U>
        using comparable
            = decltype(std::declval<const Compare&>()(std::declval<T>(), std::declval<U>()));

        const Compare& comparer() const noexcept { return *this; }

        alloc_t&       alloc() noexcept { return *this; }
        const alloc_t& alloc() const noexcept { return *this; }

        template<typename T, typename U>
        bool compare(T&& t, U&& u) const noexcept(noexcept(bool(comparer()((T &&) t, (U &&) u))))
        {
            return bool(comparer()((T &&) t, (U &&) u));
        }

        node_t* grandparent(const transaction tx, node_t* n)
        {
            if (n == nullptr)
                return nullptr;

            node_t* parent = (node_t*)tx.read(n->parent_);
            if (parent == nullptr)
                return nullptr;

            return (node_t*)tx.read(parent->parent_);
        }

        node_t* uncle(const transaction tx, node_t* n)
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

        node_t* sibling(const transaction tx, node_t* parent, node_t* n)
        {
            if (parent == nullptr)
                return nullptr;

            const auto p_left = (node_t*)tx.read(parent->left_);
            if (n == p_left)
                return (node_t*)tx.read(parent->right_);
            return p_left;
        }

        void insert_case1(const transaction tx, node_t* n)
        {
            if (tx.read(n->parent_) == nullptr)
                tx.write(n->color_, detail::color::black);
            else
                insert_case2(tx, n);
        }

        void insert_case2(const transaction tx, node_t* n)
        {
            if (tx.read(((node_t*)tx.read(n->parent_))->color_) == detail::color::red)
                insert_case3(tx, n);
        }

        void insert_case3(const transaction tx, node_t* n)
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

        void insert_case4(const transaction tx, node_t* n)
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

        void insert_case5(const transaction tx, node_t* n)
        {
            node_t* g = grandparent(tx, n);

            tx.write(((node_t*)tx.read(n->parent_))->color_, detail::color::black);
            tx.write(g->color_, detail::color::red);
            if (n == tx.read(((node_t*)tx.read(n->parent_))->left_))
                rotate_right(tx, g);
            else
                rotate_left(tx, g);
        }

        void rotate_left(const transaction tx, node_t* n)
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

        void rotate_right(const transaction tx, node_t* n)
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

        void push_impl(const transaction tx, node_t* new_node)
        {
            node_t* parent = nullptr;
            node_t* next_parent;

            auto*       cur = &root_;
            const auto& key = new_node->key.unsafe_read();

            auto read_tx = tx.unsafe_checked_demote();
            while ((next_parent = (node_t*)read_tx.untracked_read(*cur))) {
                cur = compare(key, read_tx.untracked_read(next_parent->key)) ? &next_parent->left_
                                                                             : &next_parent->right_;
                parent = next_parent;
            }

            new_node->parent_.unsafe_write(parent);
            tx.write(*cur, new_node);
            insert_case1(tx, new_node);
        }

        void replace_node(const transaction tx, node_t* n, node_t* child)
        {
            auto parent = (node_t*)tx.read(n->parent_);
            if (parent) {
                if (tx.read(parent->left_) == n)
                    tx.write(parent->left_, child);
                else
                    tx.write(parent->right_, child);
            } else {
                tx.write(root_, child);
            }
            if (child)
                tx.write(child->parent_, parent);
        }

        static detail::color color(const transaction tx, node_t* n)
        {
            return n ? tx.read(n->color_) : detail::color::black;
        }

        void delete_case6(const transaction tx, node_t* parent, node_t* n)
        {
            auto s = sibling(tx, parent, n);

            tx.write(s->color_, tx.read(parent->color_));
            tx.write(parent->color_, detail::color::black);

            if (n == tx.read(parent->left_)) {
                tx.write(((node_t*)tx.read(s->right_))->color_, detail::color::black);
                rotate_left(tx, parent);
            } else {
                tx.write(((node_t*)tx.read(s->left_))->color_, detail::color::black);
                rotate_right(tx, parent);
            }
        }

        void delete_case5(const transaction tx, node_t* parent, node_t* n)
        {
            auto s = sibling(tx, parent, n);

            if (tx.read(s->color_) == detail::color::black) { /* this if statement is trivial,
          due to case 2 (even though case 2 changed the sibling to a sibling's child,
          the sibling's child can't be red, since no red parent can have a red child). */
                /* the following statements just force the red to be on the left of the left of the
                   parent,
                   or right of the right, so case six will rotate correctly. */
                auto s_left  = (node_t*)tx.read(s->left_);
                auto s_right = (node_t*)tx.read(s->right_);
                if (n == tx.read(parent->left_) && color(tx, s_right) == detail::color::black
                    && (color(tx, s_left) == detail::color::red)) {
                    // this last test is trivial too due to cases 2-4.
                    tx.write(s->color_, detail::color::red);
                    tx.write(s_left->color_, detail::color::black);
                    rotate_right(tx, s);
                } else if (n == tx.read(parent->right_) && color(tx, s_left) == detail::color::black
                           && color(tx, s_right) == detail::color::red) {
                    // this last test is trivial too due to cases 2-4.
                    tx.write(s->color_, detail::color::red);
                    tx.write(s_right->color_, detail::color::black);
                    rotate_left(tx, s);
                }
            }
            delete_case6(tx, parent, n);
        }

        void delete_case4(const transaction tx, node_t* parent, node_t* n)
        {
            node_t* s = sibling(tx, parent, n);

            if (tx.read(parent->color_) == detail::color::red
                && tx.read(s->color_) == detail::color::black
                && color(tx, (node_t*)tx.read(s->left_)) == detail::color::black
                && color(tx, (node_t*)tx.read(s->right_)) == detail::color::black) {
                tx.write(s->color_, detail::color::red);
                tx.write(parent->color_, detail::color::black);
            } else {
                delete_case5(tx, parent, n);
            }
        }

        void delete_case3(const transaction tx, node_t* parent, node_t* n)
        {
            node_t* s = sibling(tx, parent, n);

            if (tx.read(parent->color_) == detail::color::black
                && color(tx, s) == detail::color::black
                && (!s || color(tx, (node_t*)tx.read(s->left_)) == detail::color::black)
                && (!s || color(tx, (node_t*)tx.read(s->right_)) == detail::color::black)) {
                tx.write(s->color_, detail::color::red);
                delete_case1(tx, (node_t*)tx.read(parent->parent_), parent);
            } else {
                delete_case4(tx, parent, n);
            }
        }

        void delete_case2(const transaction tx, node_t* parent, node_t* n)
        {
            auto s = sibling(tx, parent, n);
            if (color(tx, s) == detail::color::red) {
                tx.write(parent->color_, detail::color::red);
                tx.write(s->color_, detail::color::black);
                if (n == tx.read(parent->left_))
                    rotate_left(tx, parent);
                else
                    rotate_right(tx, parent);
            }
            delete_case3(tx, parent, n);
        }

        void delete_case1(const transaction tx, node_t* parent, node_t* n)
        {
            if (parent)
                delete_case2(tx, parent, n);
        }

        void delete_one_child(const transaction tx, node_t* n)
        {
            node_t* right = (node_t*)tx.read(n->right_);
            node_t* child = !right ? (node_t*)tx.read(n->left_) : right;

            replace_node(tx, n, child);
            if (tx.read(n->color_) == detail::color::black) {
                if (color(tx, child) == detail::color::red)
                    tx.write(child->color_, detail::color::black);
                else
                    delete_case1(tx, (node_t*)tx.read(n->parent_), child);
            }
            lstm::destroy_deallocate(alloc(), n);
        }

        void erase_impl(const transaction tx, node_t* to_erase)
        {
            if (tx.read(to_erase->left_) && tx.read(to_erase->right_)) {
                node_t* temp = min_node(tx, (node_t*)tx.read(to_erase->right_));

                tx.write(to_erase->key, tx.read(temp->key));
                tx.write(to_erase->value, tx.read(temp->value));
                to_erase = temp;
            }

            delete_one_child(tx, to_erase);
        }

        node_t* min_node(const transaction tx, node_t* current) const
        {
            node_t* next;
            /* loop down to find the leftmost leaf */
            while ((next = (node_t*)tx.read(current->left_)))
                current = next;

            return current;
        }

#ifndef NDEBUG
        detail::height_info minmax_height(const transaction tx, node_t* node) const
        {
            if (!node)
                return {0, 0, 0};

            auto left     = (node_t*)tx.read(node->left_);
            auto right    = (node_t*)tx.read(node->right_);
            auto height_l = minmax_height(tx, left);
            auto height_r = minmax_height(tx, right);

            LSTM_ASSERT(height_l.black_height == height_r.black_height);

            LSTM_ASSERT(!left || tx.read(left->parent_) == node);
            LSTM_ASSERT(!right || tx.read(right->parent_) == node);

            LSTM_ASSERT(!left || !compare(tx.read(node->key), tx.read(left->key)));
            LSTM_ASSERT(!right || !compare(tx.read(right->key), tx.read(node->key)));

            if (tx.read(node->color_) == detail::color::red) {
                LSTM_ASSERT(!left || tx.read(left->color_) == detail::color::black);
                LSTM_ASSERT(!right || tx.read(right->color_) == detail::color::black);
            }

            return {std::min(height_l.min_height, height_r.min_height) + 1,
                    std::max(height_l.max_height, height_r.max_height) + 1,
                    height_l.black_height
                        + (tx.read(node->color_) == detail::color::black ? 1 : 0)};
        }
#endif

        static void destroy_deallocate_subtree(alloc_t alloc, node_t* node)
        {
            while (node->left_.unsafe_read()) {
                node_t* parent = node;
                auto    leaf   = (node_t*)node->left_.unsafe_read();
                while (leaf->left_.unsafe_read()) {
                    parent = leaf;
                    leaf   = (node_t*)leaf->left_.unsafe_read();
                }
                parent->left_.unsafe_write(parent->right_.unsafe_read());
                parent->right_.unsafe_write(nullptr);
                alloc_traits::destroy(alloc, leaf);
                alloc_traits::deallocate(alloc, leaf, 1);
            }
            alloc_traits::destroy(alloc, node);
            alloc_traits::deallocate(alloc, node, 1);
        }

    public:
        rbtree(const Alloc& alloc = {}) noexcept
            : alloc_t(alloc)
            , root_{std::allocator_arg, alloc, nullptr}
        {
        }

        // TODO: these
        rbtree(const rbtree&) = delete;
        rbtree& operator=(const rbtree&) = delete;
        ~rbtree()
        {
            if (auto root = (node_t*)root_.unsafe_read())
                destroy_deallocate_subtree(alloc(), root);
        }

        void clear(const transaction tx)
        {
            if (auto root = (node_t*)tx.read(root_)) {
                tx.write(root_, nullptr);
                lstm::tls_thread_data().queue_succ_callback(
                    [ alloc = this->alloc(), root ]() noexcept {
                        destroy_deallocate_subtree(alloc, root);
                    });
            }
        }

        node_t* find(const read_transaction tx, const Key& u) const
        {
            node_t* cur = (node_t*)tx.untracked_read(root_);
            while (cur) {
                const auto& key = tx.untracked_read(cur->key);
                if (compare(u, key))
                    cur = (node_t*)tx.untracked_read(cur->left_);
                else if (compare(key, u))
                    cur = (node_t*)tx.untracked_read(cur->right_);
                else
                    break;
            }
            return cur;
        }

        template<typename... Us, LSTM_REQUIRES_(std::is_constructible<node_t, alloc_t&, Us&&...>{})>
        void emplace(const transaction tx, Us&&... us)
        {
            push_impl(tx, lstm::allocate_construct(alloc(), alloc(), (Us &&) us...));
        }

        bool erase_one(const transaction tx, const Key& key)
        {
            if (auto to_erase = find(tx.unsafe_checked_demote(), key)) {
                erase_impl(tx, to_erase);
                return true;
            }
            return false;
        }

#ifndef NDEBUG
        void verify(const transaction tx) const
        {
            auto root = (node_t*)tx.read(root_);
            if (root)
                LSTM_ASSERT(tx.read(root->color_) == detail::color::black);
            const detail::height_info heights = minmax_height(tx, root);
            (void)heights;
            LSTM_ASSERT(heights.min_height * 2 >= heights.max_height);
        }
#endif
    };
LSTM_END

#endif /* LSTM_CONTAINERS_RBTREE_HPP */
