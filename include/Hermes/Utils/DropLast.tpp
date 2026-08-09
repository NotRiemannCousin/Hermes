#pragma once
#include <type_traits>

namespace Hermes::Utils {

    template<rg::range Range>
    DropLastView<Range>::Iterator::Iterator(DropLastView *parent) : m_view(parent) {
        if (m_view->m_current != rg::end(m_view->m_view)) {
            m_view->m_val = *m_view->m_current;
            ++m_view->m_current;
        }
    }

    template<rg::range Range>
    typename DropLastView<Range>::Iterator&
    DropLastView<Range>::Iterator::operator++() {
        if (m_view->m_current != rg::end(m_view->m_view)) {
            m_view->m_val = *m_view->m_current;
            ++m_view->m_current;
        }
        return *this;
    }

    template<rg::range Range>
    typename DropLastView<Range>::Type
    DropLastView<Range>::Iterator::operator*() const {
        return m_view->m_val;
    }

    template<rg::range Range>
    bool DropLastView<Range>::Iterator::operator==(std::default_sentinel_t) const {
        return m_view->m_current == rg::end(m_view->m_view);
    }

    template<rg::range Range>
    void DropLastView<Range>::Iterator::operator++(int) {
        ++*this;
    }

    template<rg::range Range>
    DropLastView<Range>::DropLastView(Range base)
        : m_view{ std::move(base) } {
        m_current = rg::begin(m_view);
    }

    template<rg::range Range>
    typename DropLastView<Range>::Iterator DropLastView<Range>::begin() {
        return Iterator{ this };
    }

    template<rg::range Range>
    std::default_sentinel_t DropLastView<Range>::end() {
        return {};
    }

    template<rg::range R>
    constexpr auto DropLastAdaptor::operator()(R&& r) const {
        return DropLastView<vs::all_t<R>>(vs::all(std::forward<R>(r)));
    }

    template<rg::range Range>
    constexpr auto operator|(Range&& r, const DropLastAdaptor& adaptor) {
        return adaptor(std::forward<Range>(r));
    }

}