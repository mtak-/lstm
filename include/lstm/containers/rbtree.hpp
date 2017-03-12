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

            node_t* parent = (node_t*)n->parent_.get(tx);
            if (parent == nullptr)
                return nullptr;

            return (node_t*)parent->parent_.get(tx);
        }

        node_t* uncle(const transaction tx, node_t* n)
        {
            node_t* g = grandparent(tx, n);
            if (g == nullptr)
                return nullptr;

            node_t* left = (node_t*)g->left_.get(tx);
            if (n->parent_.get(tx) == left)
                return (node_t*)g->right_.get(tx);
            else
                return left;
        }

        node_t* sibling(const transaction tx, node_t* parent, node_t* n)
        {
            if (parent == nullptr)
                return nullptr;

            const auto p_left = (node_t*)parent->left_.get(tx);
            if (n == p_left)
                return (node_t*)parent->right_.get(tx);
            return p_left;
        }

        void insert_case1(const transaction tx, node_t* n)
        {
            if (n->parent_.get(tx) == nullptr)
                n->color_.set(tx, detail::color::black);
            else
                insert_case2(tx, n);
        }

        void insert_case2(const transaction tx, node_t* n)
        {
            if (((node_t*)n->parent_.get(tx))->color_.get(tx) == detail::color::red)
                insert_case3(tx, n);
        }

        void insert_case3(const transaction tx, node_t* n)
        {
            node_t* u = uncle(tx, n);

            if (u != nullptr && u->color_.get(tx) == detail::color::red) {
                ((node_t*)n->parent_.get(tx))->color_.set(tx, detail::color::black);
                u->color_.set(tx, detail::color::black);
                node_t* g = grandparent(tx, n);
                g->color_.set(tx, detail::color::red);
                insert_case1(tx, g);
            } else {
                insert_case4(tx, n);
            }
        }

        void insert_case4(const transaction tx, node_t* n)
        {
            node_t* g = grandparent(tx, n);

            if (n == ((node_t*)n->parent_.get(tx))->right_.get(tx)
                && n->parent_.get(tx) == g->left_.get(tx)) {
                // rotate_left(n->parent);

                node_t* saved_p      = (node_t*)g->left_.get(tx);
                node_t* saved_left_n = (node_t*)n->left_.get(tx);

                g->left_.set(tx, n);
                n->parent_.set(tx, g);

                n->left_.set(tx, saved_p);
                saved_p->parent_.set(tx, n);

                saved_p->right_.set(tx, saved_left_n);
                if (saved_left_n)
                    saved_left_n->parent_.set(tx, saved_p);

                n = saved_p;
            } else if (n == ((node_t*)n->parent_.get(tx))->left_.get(tx)
                       && n->parent_.get(tx) == g->right_.get(tx)) {
                // rotate_right(n->parent);

                node_t* saved_p       = (node_t*)g->right_.get(tx);
                node_t* saved_right_n = (node_t*)n->right_.get(tx);

                g->right_.set(tx, n);
                n->parent_.set(tx, g);

                n->right_.set(tx, saved_p);
                saved_p->parent_.set(tx, n);

                saved_p->left_.set(tx, saved_right_n);
                if (saved_right_n)
                    saved_right_n->parent_.set(tx, saved_p);

                n = saved_p;
            }
            insert_case5(tx, n);
        }

        void insert_case5(const transaction tx, node_t* n)
        {
            node_t* g = grandparent(tx, n);

            ((node_t*)n->parent_.get(tx))->color_.set(tx, detail::color::black);
            g->color_.set(tx, detail::color::red);
            if (n == ((node_t*)n->parent_.get(tx))->left_.get(tx))
                rotate_right(tx, g);
            else
                rotate_left(tx, g);
        }

        void rotate_left(const transaction tx, node_t* n)
        {
            node_t* p        = (node_t*)n->right_.get(tx);
            node_t* p_left   = (node_t*)p->left_.get(tx);
            node_t* n_parent = (node_t*)n->parent_.get(tx);
            bool    left     = n_parent && n_parent->left_.get(tx) == n;
            n->right_.set(tx, p_left);

            if (p_left)
                p_left->parent_.set(tx, n);

            p->left_.set(tx, n);
            n->parent_.set(tx, p);

            p->parent_.set(tx, n_parent);
            if (!n_parent)
                root_.set(tx, p);
            else if (left)
                n_parent->left_.set(tx, p);
            else
                n_parent->right_.set(tx, p);
        }

        void rotate_right(const transaction tx, node_t* n)
        {
            node_t* p        = (node_t*)n->left_.get(tx);
            node_t* p_right  = (node_t*)p->right_.get(tx);
            node_t* n_parent = (node_t*)n->parent_.get(tx);
            bool    left     = n_parent && n_parent->left_.get(tx) == n;
            n->left_.set(tx, p_right);

            if (p_right)
                p_right->parent_.set(tx, n);

            p->right_.set(tx, n);
            n->parent_.set(tx, p);

            p->parent_.set(tx, n_parent);
            if (!n_parent)
                root_.set(tx, p);
            else if (left)
                n_parent->left_.set(tx, p);
            else
                n_parent->right_.set(tx, p);
        }

        void push_impl(const transaction tx, node_t* new_node)
        {
            node_t* parent = nullptr;
            node_t* next_parent;

            auto*       cur = &root_;
            const auto& key = new_node->key.unsafe_get();

            auto read_tx = tx.unsafe_checked_demote();
            while ((next_parent = (node_t*)cur->untracked_get(read_tx))) {
                cur = compare(key, next_parent->key.untracked_get(read_tx)) ? &next_parent->left_
                                                                            : &next_parent->right_;
                parent = next_parent;
            }
            auto root = root_.get(tx);
            if (parent && parent != root) {
                parent->key.get(tx);
                parent->left_.get(tx);
                parent->right_.get(tx);
                auto pp = (node_t*)parent->parent_.get(tx);
                if (pp && pp != root) {
                    pp->left_.get(tx);
                    pp->right_.get(tx);
                }
            }

            new_node->parent_.unsafe_set(parent);
            cur->set(tx, new_node);
            insert_case1(tx, new_node);
        }

        void replace_node(const transaction tx, node_t* n, node_t* child)
        {
            auto parent = (node_t*)n->parent_.get(tx);
            if (parent) {
                if (parent->left_.get(tx) == n)
                    parent->left_.set(tx, child);
                else
                    parent->right_.set(tx, child);
            } else {
                root_.set(tx, child);
            }
            if (child)
                child->parent_.set(tx, parent);
        }

        static detail::color color(const transaction tx, node_t* n)
        {
            return n ? n->color_.get(tx) : detail::color::black;
        }

        void delete_case6(const transaction tx, node_t* parent, node_t* n)
        {
            auto s = sibling(tx, parent, n);

            s->color_.set(tx, parent->color_.get(tx));
            parent->color_.set(tx, detail::color::black);

            if (n == parent->left_.get(tx)) {
                ((node_t*)s->right_.get(tx))->color_.set(tx, detail::color::black);
                rotate_left(tx, parent);
            } else {
                ((node_t*)s->left_.get(tx))->color_.set(tx, detail::color::black);
                rotate_right(tx, parent);
            }
        }

        void delete_case5(const transaction tx, node_t* parent, node_t* n)
        {
            auto s = sibling(tx, parent, n);

            if (s->color_.get(tx) == detail::color::black) { /* this if statement is trivial,
          due to case 2 (even though case 2 changed the sibling to a sibling's child,
          the sibling's child can't be red, since no red parent can have a red child). */
                /* the following statements just force the red to be on the left of the left of the
                   parent,
                   or right of the right, so case six will rotate correctly. */
                auto s_left  = (node_t*)s->left_.get(tx);
                auto s_right = (node_t*)s->right_.get(tx);
                if (n == parent->left_.get(tx) && color(tx, s_right) == detail::color::black
                    && (color(tx, s_left) == detail::color::red)) {
                    // this last test is trivial too due to cases 2-4.
                    s->color_.set(tx, detail::color::red);
                    s_left->color_.set(tx, detail::color::black);
                    rotate_right(tx, s);
                } else if (n == parent->right_.get(tx) && color(tx, s_left) == detail::color::black
                           && color(tx, s_right) == detail::color::red) {
                    // this last test is trivial too due to cases 2-4.
                    s->color_.set(tx, detail::color::red);
                    s_right->color_.set(tx, detail::color::black);
                    rotate_left(tx, s);
                }
            }
            delete_case6(tx, parent, n);
        }

        void delete_case4(const transaction tx, node_t* parent, node_t* n)
        {
            node_t* s = sibling(tx, parent, n);

            if (parent->color_.get(tx) == detail::color::red
                && s->color_.get(tx) == detail::color::black
                && color(tx, (node_t*)s->left_.get(tx)) == detail::color::black
                && color(tx, (node_t*)s->right_.get(tx)) == detail::color::black) {
                s->color_.set(tx, detail::color::red);
                parent->color_.set(tx, detail::color::black);
            } else {
                delete_case5(tx, parent, n);
            }
        }

        void delete_case3(const transaction tx, node_t* parent, node_t* n)
        {
            node_t* s = sibling(tx, parent, n);

            if (parent->color_.get(tx) == detail::color::black
                && color(tx, s) == detail::color::black
                && (!s || color(tx, (node_t*)s->left_.get(tx)) == detail::color::black)
                && (!s || color(tx, (node_t*)s->right_.get(tx)) == detail::color::black)) {
                s->color_.set(tx, detail::color::red);
                delete_case1(tx, (node_t*)parent->parent_.get(tx), parent);
            } else {
                delete_case4(tx, parent, n);
            }
        }

        void delete_case2(const transaction tx, node_t* parent, node_t* n)
        {
            auto s = sibling(tx, parent, n);
            if (color(tx, s) == detail::color::red) {
                parent->color_.set(tx, detail::color::red);
                s->color_.set(tx, detail::color::black);
                if (n == parent->left_.get(tx))
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
            node_t* right = (node_t*)n->right_.get(tx);
            node_t* child = !right ? (node_t*)n->left_.get(tx) : right;

            replace_node(tx, n, child);
            if (n->color_.get(tx) == detail::color::black) {
                if (color(tx, child) == detail::color::red)
                    child->color_.set(tx, detail::color::black);
                else
                    delete_case1(tx, (node_t*)n->parent_.get(tx), child);
            }
            lstm::destroy_deallocate(alloc(), n);
        }

        void erase_impl(const transaction tx, node_t* to_erase)
        {
            if (to_erase->left_.get(tx) && to_erase->right_.get(tx)) {
                node_t* temp = min_node(tx, (node_t*)to_erase->right_.get(tx));

                to_erase->key.set(tx, temp->key.get(tx));
                to_erase->value.set(tx, temp->value.get(tx));
                to_erase = temp;
            }

            delete_one_child(tx, to_erase);
        }

        node_t* min_node(const transaction tx, node_t* current) const
        {
            node_t* next;
            /* loop down to find the leftmost leaf */
            while ((next = (node_t*)current->left_.get(tx)))
                current = next;

            return current;
        }

#ifndef NDEBUG
        detail::height_info minmax_height(const transaction tx, node_t* node) const
        {
            if (!node)
                return {0, 0, 0};

            auto left     = (node_t*)node->left_.get(tx);
            auto right    = (node_t*)node->right_.get(tx);
            auto height_l = minmax_height(tx, left);
            auto height_r = minmax_height(tx, right);

            LSTM_ASSERT(height_l.black_height == height_r.black_height);

            LSTM_ASSERT(!left || left->parent_.get(tx) == node);
            LSTM_ASSERT(!right || right->parent_.get(tx) == node);

            LSTM_ASSERT(!left || !compare(node->key.get(tx), left->key.get(tx)));
            LSTM_ASSERT(!right || !compare(right->key.get(tx), node->key.get(tx)));

            if (node->color_.get(tx) == detail::color::red) {
                LSTM_ASSERT(!left || left->color_.get(tx) == detail::color::black);
                LSTM_ASSERT(!right || right->color_.get(tx) == detail::color::black);
            }

            return {std::min(height_l.min_height, height_r.min_height) + 1,
                    std::max(height_l.max_height, height_r.max_height) + 1,
                    height_l.black_height + (node->color_.get(tx) == detail::color::black ? 1 : 0)};
        }
#endif

        static void destroy_deallocate_subtree(alloc_t alloc, node_t* node)
        {
            while (node->left_.unsafe_get()) {
                node_t* parent = node;
                auto    leaf   = (node_t*)node->left_.unsafe_get();
                while (leaf->left_.unsafe_get()) {
                    parent = leaf;
                    leaf   = (node_t*)leaf->left_.unsafe_get();
                }
                parent->left_.unsafe_set(parent->right_.unsafe_get());
                parent->right_.unsafe_set(nullptr);
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
            if (auto root = (node_t*)root_.unsafe_get())
                destroy_deallocate_subtree(alloc(), root);
        }

        void clear(const transaction tx)
        {
            if (auto root = (node_t*)root_.get(tx)) {
                root_.set(tx, nullptr);
                lstm::tls_thread_data().sometime_after([ alloc = this->alloc(), root ]() noexcept {
                    destroy_deallocate_subtree(alloc, root);
                });
            }
        }

        node_t* find(const read_transaction tx, const Key& u) const
        {
            node_t* cur = (node_t*)root_.untracked_get(tx);
            while (cur) {
                const auto& key = cur->key.untracked_get(tx);
                if (compare(u, key))
                    cur = (node_t*)cur->left_.untracked_get(tx);
                else if (compare(key, u))
                    cur = (node_t*)cur->right_.untracked_get(tx);
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
            auto root = (node_t*)root_.get(tx);
            if (root)
                LSTM_ASSERT(root->color_.get(tx) == detail::color::black);
            const detail::height_info heights = minmax_height(tx, root);
            (void)heights;
            LSTM_ASSERT(heights.min_height * 2 >= heights.max_height);
        }
#endif
    };
LSTM_END

#endif /* LSTM_CONTAINERS_RBTREE_HPP */
