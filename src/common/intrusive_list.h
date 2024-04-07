// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/parent_of_member.h"

namespace Common {

// Forward declare implementation class for Node.
namespace impl {

class IntrusiveListImpl;

}

class IntrusiveListNode {
    SUDACHI_NON_COPYABLE(IntrusiveListNode);

private:
    friend class impl::IntrusiveListImpl;

    IntrusiveListNode* m_prev;
    IntrusiveListNode* m_next;

public:
    constexpr IntrusiveListNode() : m_prev(this), m_next(this) {}

    constexpr bool IsLinked() const {
        return m_next != this;
    }

private:
    constexpr void LinkPrev(IntrusiveListNode* node) {
        // We can't link an already linked node.
        ASSERT(!node->IsLinked());
        this->SplicePrev(node, node);
    }

    constexpr void SplicePrev(IntrusiveListNode* first, IntrusiveListNode* last) {
        // Splice a range into the list.
        auto last_prev = last->m_prev;
        first->m_prev = m_prev;
        last_prev->m_next = this;
        m_prev->m_next = first;
        m_prev = last_prev;
    }

    constexpr void LinkNext(IntrusiveListNode* node) {
        // We can't link an already linked node.
        ASSERT(!node->IsLinked());
        return this->SpliceNext(node, node);
    }

    constexpr void SpliceNext(IntrusiveListNode* first, IntrusiveListNode* last) {
        // Splice a range into the list.
        auto last_prev = last->m_prev;
        first->m_prev = this;
        last_prev->m_next = m_next;
        m_next->m_prev = last_prev;
        m_next = first;
    }

    constexpr void Unlink() {
        this->Unlink(m_next);
    }

    constexpr void Unlink(IntrusiveListNode* last) {
        // Unlink a node from a next node.
        auto last_prev = last->m_prev;
        m_prev->m_next = last;
        last->m_prev = m_prev;
        last_prev->m_next = this;
        m_prev = last_prev;
    }

    constexpr IntrusiveListNode* GetPrev() {
        return m_prev;
    }

    constexpr const IntrusiveListNode* GetPrev() const {
        return m_prev;
    }

    constexpr IntrusiveListNode* GetNext() {
        return m_next;
    }

    constexpr const IntrusiveListNode* GetNext() const {
        return m_next;
    }
};
// DEPRECATED: static_assert(std::is_literal_type<IntrusiveListNode>::value);

namespace impl {

class IntrusiveListImpl {
    SUDACHI_NON_COPYABLE(IntrusiveListImpl);

private:
    IntrusiveListNode m_root_node;

public:
    template <bool Const>
    class Iterator;

    using value_type = IntrusiveListNode;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    template <bool Const>
    class Iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = typename IntrusiveListImpl::value_type;
        using difference_type = typename IntrusiveListImpl::difference_type;
        using pointer =
            std::conditional_t<Const, IntrusiveListImpl::const_pointer, IntrusiveListImpl::pointer>;
        using reference = std::conditional_t<Const, IntrusiveListImpl::const_reference,
                                             IntrusiveListImpl::reference>;

    private:
        pointer m_node;

    public:
        constexpr explicit Iterator(pointer n) : m_node(n) {}

        constexpr bool operator==(const Iterator& rhs) const {
            return m_node == rhs.m_node;
        }

        constexpr pointer operator->() const {
            return m_node;
        }

        constexpr reference operator*() const {
            return *m_node;
        }

        constexpr Iterator& operator++() {
            m_node = m_node->m_next;
            return *this;
        }

        constexpr Iterator& operator--() {
            m_node = m_node->m_prev;
            return *this;
        }

        constexpr Iterator operator++(int) {
            const Iterator it{*this};
            ++(*this);
            return it;
        }

        constexpr Iterator operator--(int) {
            const Iterator it{*this};
            --(*this);
            return it;
        }

        constexpr operator Iterator<true>() const {
            return Iterator<true>(m_node);
        }

        constexpr Iterator<false> GetNonConstIterator() const {
            return Iterator<false>(const_cast<IntrusiveListImpl::pointer>(m_node));
        }
    };

public:
    constexpr IntrusiveListImpl() : m_root_node() {}

    // Iterator accessors.
    constexpr iterator begin() {
        return iterator(m_root_node.GetNext());
    }

    constexpr const_iterator begin() const {
        return const_iterator(m_root_node.GetNext());
    }

    constexpr iterator end() {
        return iterator(std::addressof(m_root_node));
    }

    constexpr const_iterator end() const {
        return const_iterator(std::addressof(m_root_node));
    }

    constexpr iterator iterator_to(reference v) {
        // Only allow iterator_to for values in lists.
        ASSERT(v.IsLinked());
        return iterator(std::addressof(v));
    }

    constexpr const_iterator iterator_to(const_reference v) const {
        // Only allow iterator_to for values in lists.
        ASSERT(v.IsLinked());
        return const_iterator(std::addressof(v));
    }

    // Content management.
    constexpr bool empty() const {
        return !m_root_node.IsLinked();
    }

    constexpr size_type size() const {
        return static_cast<size_type>(std::distance(this->begin(), this->end()));
    }

    constexpr reference back() {
        return *m_root_node.GetPrev();
    }

    constexpr const_reference back() const {
        return *m_root_node.GetPrev();
    }

    constexpr reference front() {
        return *m_root_node.GetNext();
    }

    constexpr const_reference front() const {
        return *m_root_node.GetNext();
    }

    constexpr void push_back(reference node) {
        m_root_node.LinkPrev(std::addressof(node));
    }

    constexpr void push_front(reference node) {
        m_root_node.LinkNext(std::addressof(node));
    }

    constexpr void pop_back() {
        m_root_node.GetPrev()->Unlink();
    }

    constexpr void pop_front() {
        m_root_node.GetNext()->Unlink();
    }

    constexpr iterator insert(const_iterator pos, reference node) {
        pos.GetNonConstIterator()->LinkPrev(std::addressof(node));
        return iterator(std::addressof(node));
    }

    constexpr void splice(const_iterator pos, IntrusiveListImpl& o) {
        splice_impl(pos, o.begin(), o.end());
    }

    constexpr void splice(const_iterator pos, IntrusiveListImpl& o, const_iterator first) {
        const_iterator last(first);
        std::advance(last, 1);
        splice_impl(pos, first, last);
    }

    constexpr void splice(const_iterator pos, IntrusiveListImpl& o, const_iterator first,
                          const_iterator last) {
        splice_impl(pos, first, last);
    }

    constexpr iterator erase(const_iterator pos) {
        if (pos == this->end()) {
            return this->end();
        }
        iterator it(pos.GetNonConstIterator());
        (it++)->Unlink();
        return it;
    }

    constexpr void clear() {
        while (!this->empty()) {
            this->pop_front();
        }
    }

private:
    constexpr void splice_impl(const_iterator _pos, const_iterator _first, const_iterator _last) {
        if (_first == _last) {
            return;
        }
        iterator pos(_pos.GetNonConstIterator());
        iterator first(_first.GetNonConstIterator());
        iterator last(_last.GetNonConstIterator());
        first->Unlink(std::addressof(*last));
        pos->SplicePrev(std::addressof(*first), std::addressof(*first));
    }
};

} // namespace impl

template <class T, class Traits>
class IntrusiveList {
    SUDACHI_NON_COPYABLE(IntrusiveList);

private:
    impl::IntrusiveListImpl m_impl;

public:
    template <bool Const>
    class Iterator;

    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    template <bool Const>
    class Iterator {
    public:
        friend class Common::IntrusiveList<T, Traits>;

        using ImplIterator =
            std::conditional_t<Const, Common::impl::IntrusiveListImpl::const_iterator,
                               Common::impl::IntrusiveListImpl::iterator>;

        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = typename IntrusiveList::value_type;
        using difference_type = typename IntrusiveList::difference_type;
        using pointer =
            std::conditional_t<Const, IntrusiveList::const_pointer, IntrusiveList::pointer>;
        using reference =
            std::conditional_t<Const, IntrusiveList::const_reference, IntrusiveList::reference>;

    private:
        ImplIterator m_iterator;

    private:
        constexpr explicit Iterator(ImplIterator it) : m_iterator(it) {}

        constexpr ImplIterator GetImplIterator() const {
            return m_iterator;
        }

    public:
        constexpr bool operator==(const Iterator& rhs) const {
            return m_iterator == rhs.m_iterator;
        }

        constexpr pointer operator->() const {
            return std::addressof(Traits::GetParent(*m_iterator));
        }

        constexpr reference operator*() const {
            return Traits::GetParent(*m_iterator);
        }

        constexpr Iterator& operator++() {
            ++m_iterator;
            return *this;
        }

        constexpr Iterator& operator--() {
            --m_iterator;
            return *this;
        }

        constexpr Iterator operator++(int) {
            const Iterator it{*this};
            ++m_iterator;
            return it;
        }

        constexpr Iterator operator--(int) {
            const Iterator it{*this};
            --m_iterator;
            return it;
        }

        constexpr operator Iterator<true>() const {
            return Iterator<true>(m_iterator);
        }
    };

private:
    static constexpr IntrusiveListNode& GetNode(reference ref) {
        return Traits::GetNode(ref);
    }

    static constexpr IntrusiveListNode const& GetNode(const_reference ref) {
        return Traits::GetNode(ref);
    }

    static constexpr reference GetParent(IntrusiveListNode& node) {
        return Traits::GetParent(node);
    }

    static constexpr const_reference GetParent(IntrusiveListNode const& node) {
        return Traits::GetParent(node);
    }

public:
    constexpr IntrusiveList() : m_impl() {}

    // Iterator accessors.
    constexpr iterator begin() {
        return iterator(m_impl.begin());
    }

    constexpr const_iterator begin() const {
        return const_iterator(m_impl.begin());
    }

    constexpr iterator end() {
        return iterator(m_impl.end());
    }

    constexpr const_iterator end() const {
        return const_iterator(m_impl.end());
    }

    constexpr const_iterator cbegin() const {
        return this->begin();
    }

    constexpr const_iterator cend() const {
        return this->end();
    }

    constexpr reverse_iterator rbegin() {
        return reverse_iterator(this->end());
    }

    constexpr const_reverse_iterator rbegin() const {
        return const_reverse_iterator(this->end());
    }

    constexpr reverse_iterator rend() {
        return reverse_iterator(this->begin());
    }

    constexpr const_reverse_iterator rend() const {
        return const_reverse_iterator(this->begin());
    }

    constexpr const_reverse_iterator crbegin() const {
        return this->rbegin();
    }

    constexpr const_reverse_iterator crend() const {
        return this->rend();
    }

    constexpr iterator iterator_to(reference v) {
        return iterator(m_impl.iterator_to(GetNode(v)));
    }

    constexpr const_iterator iterator_to(const_reference v) const {
        return const_iterator(m_impl.iterator_to(GetNode(v)));
    }

    // Content management.
    constexpr bool empty() const {
        return m_impl.empty();
    }

    constexpr size_type size() const {
        return m_impl.size();
    }

    constexpr reference back() {
        return GetParent(m_impl.back());
    }

    constexpr const_reference back() const {
        return GetParent(m_impl.back());
    }

    constexpr reference front() {
        return GetParent(m_impl.front());
    }

    constexpr const_reference front() const {
        return GetParent(m_impl.front());
    }

    constexpr void push_back(reference ref) {
        m_impl.push_back(GetNode(ref));
    }

    constexpr void push_front(reference ref) {
        m_impl.push_front(GetNode(ref));
    }

    constexpr void pop_back() {
        m_impl.pop_back();
    }

    constexpr void pop_front() {
        m_impl.pop_front();
    }

    constexpr iterator insert(const_iterator pos, reference ref) {
        return iterator(m_impl.insert(pos.GetImplIterator(), GetNode(ref)));
    }

    constexpr void splice(const_iterator pos, IntrusiveList& o) {
        m_impl.splice(pos.GetImplIterator(), o.m_impl);
    }

    constexpr void splice(const_iterator pos, IntrusiveList& o, const_iterator first) {
        m_impl.splice(pos.GetImplIterator(), o.m_impl, first.GetImplIterator());
    }

    constexpr void splice(const_iterator pos, IntrusiveList& o, const_iterator first,
                          const_iterator last) {
        m_impl.splice(pos.GetImplIterator(), o.m_impl, first.GetImplIterator(),
                      last.GetImplIterator());
    }

    constexpr iterator erase(const_iterator pos) {
        return iterator(m_impl.erase(pos.GetImplIterator()));
    }

    constexpr void clear() {
        m_impl.clear();
    }
};

template <auto T, class Derived = Common::impl::GetParentType<T>>
class IntrusiveListMemberTraits;

template <class Parent, IntrusiveListNode Parent::*Member, class Derived>
class IntrusiveListMemberTraits<Member, Derived> {
public:
    using ListType = IntrusiveList<Derived, IntrusiveListMemberTraits>;

private:
    friend class IntrusiveList<Derived, IntrusiveListMemberTraits>;

    static constexpr IntrusiveListNode& GetNode(Derived& parent) {
        return parent.*Member;
    }

    static constexpr IntrusiveListNode const& GetNode(Derived const& parent) {
        return parent.*Member;
    }

    static Derived& GetParent(IntrusiveListNode& node) {
        return Common::GetParentReference<Member, Derived>(std::addressof(node));
    }

    static Derived const& GetParent(IntrusiveListNode const& node) {
        return Common::GetParentReference<Member, Derived>(std::addressof(node));
    }
};

template <auto T, class Derived = Common::impl::GetParentType<T>>
class IntrusiveListMemberTraitsByNonConstexprOffsetOf;

template <class Parent, IntrusiveListNode Parent::*Member, class Derived>
class IntrusiveListMemberTraitsByNonConstexprOffsetOf<Member, Derived> {
public:
    using ListType = IntrusiveList<Derived, IntrusiveListMemberTraitsByNonConstexprOffsetOf>;

private:
    friend class IntrusiveList<Derived, IntrusiveListMemberTraitsByNonConstexprOffsetOf>;

    static constexpr IntrusiveListNode& GetNode(Derived& parent) {
        return parent.*Member;
    }

    static constexpr IntrusiveListNode const& GetNode(Derived const& parent) {
        return parent.*Member;
    }

    static Derived& GetParent(IntrusiveListNode& node) {
        return *reinterpret_cast<Derived*>(reinterpret_cast<char*>(std::addressof(node)) -
                                           GetOffset());
    }

    static Derived const& GetParent(IntrusiveListNode const& node) {
        return *reinterpret_cast<const Derived*>(
            reinterpret_cast<const char*>(std::addressof(node)) - GetOffset());
    }

    static uintptr_t GetOffset() {
        return reinterpret_cast<uintptr_t>(std::addressof(reinterpret_cast<Derived*>(0)->*Member));
    }
};

template <class Derived>
class IntrusiveListBaseNode : public IntrusiveListNode {};

template <class Derived>
class IntrusiveListBaseTraits {
public:
    using ListType = IntrusiveList<Derived, IntrusiveListBaseTraits>;

private:
    friend class IntrusiveList<Derived, IntrusiveListBaseTraits>;

    static constexpr IntrusiveListNode& GetNode(Derived& parent) {
        return static_cast<IntrusiveListNode&>(
            static_cast<IntrusiveListBaseNode<Derived>&>(parent));
    }

    static constexpr IntrusiveListNode const& GetNode(Derived const& parent) {
        return static_cast<const IntrusiveListNode&>(
            static_cast<const IntrusiveListBaseNode<Derived>&>(parent));
    }

    static constexpr Derived& GetParent(IntrusiveListNode& node) {
        return static_cast<Derived&>(static_cast<IntrusiveListBaseNode<Derived>&>(node));
    }

    static constexpr Derived const& GetParent(IntrusiveListNode const& node) {
        return static_cast<const Derived&>(
            static_cast<const IntrusiveListBaseNode<Derived>&>(node));
    }
};

} // namespace Common
