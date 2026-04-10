#pragma once
#include <type_traits>

namespace Hermes::Utils {

    template<rg::range Range>
    DropLastView<Range>::Iterator::Iterator(DropLastView *parent) : _view(parent) {
        if (_view->_current != rg::end(_view->_view)) {
            _view->_val = *_view->_current;
            ++_view->_current;
        }
    }

    template<rg::range Range>
    typename DropLastView<Range>::Iterator&
    DropLastView<Range>::Iterator::operator++() {
        if (_view->_current != rg::end(_view->_view)) {
            _view->_val = *_view->_current;
            ++_view->_current;
        }
        return *this;
    }

    template<rg::range Range>
    typename DropLastView<Range>::Type
    DropLastView<Range>::Iterator::operator*() const {
        return _view->_val;
    }

    template<rg::range Range>
    bool DropLastView<Range>::Iterator::operator==(std::default_sentinel_t) const {
        return _view->_current == rg::end(_view->_view);
    }

    template<rg::range Range>
    void DropLastView<Range>::Iterator::operator++(int) {
        ++*this;
    }

    template<rg::range Range>
    DropLastView<Range>::DropLastView(Range base)
        : _view{ std::move(base) } {
        _current = rg::begin(_view);
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