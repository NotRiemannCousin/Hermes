#pragma once
#include <algorithm>

namespace Hermes::Utils {
    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    UntilMatchView<Range, Pattern, Inclusive>::Iterator::Iterator(UntilMatchView *parent)
        : _view(parent) {
        if (!_view->_history.empty())
            return;

        _view->_history.insert_range(_view->_history.begin(), _view->_view | vs::take(rg::size(_view->_pattern)));
        _view->_matchFound = rg::equal(_view->_history, _view->_pattern);

        if constexpr (rg::bidirectional_range<Range>)
            _view->_current = rg::next(_view->_view.begin(), rg::size(_view->_pattern));
        else
            _view->_current = _view->_view.begin();

        _view->_head += _view->_history.size();
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    typename UntilMatchView<Range, Pattern, Inclusive>::Iterator&
    UntilMatchView<Range, Pattern, Inclusive>::Iterator::operator++() {
        if (_view->_matchFound) {
            if constexpr (Inclusive)
                if (_view->_tail != _view->_head)
                    ++_view->_tail;

            return *this;
        }

        if (_view->_current != _view->_view.end()) {
            _view->_history[GetHeadIndex()] = *_view->_current;
            ++_view->_current;
            ++_view->_head;
        }

        ++_view->_tail;
        const auto s_getChar = [&](const std::size_t i) {
            return _view->_history[GetIndex(i)];
        };

        if (rg::equal(vs::iota(_view->_tail, _view->_head), _view->_pattern, {}, s_getChar))
            _view->_matchFound = true;

        return *this;
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    typename UntilMatchView<Range, Pattern, Inclusive>::Type
    UntilMatchView<Range, Pattern, Inclusive>::Iterator::operator*() const {
        return _view->_history[GetTailIndex()];
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    bool UntilMatchView<Range, Pattern, Inclusive>::Iterator::operator==(std::default_sentinel_t) const {
        if constexpr (Inclusive)
            return (_view->_matchFound || _view->_current == _view->_view.end()) && _view->_tail >= _view->_head;
        else
            return _view->_matchFound || (_view->_current == _view->_view.end() && _view->_tail >= _view->_head);
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    UntilMatchView<Range, Pattern, Inclusive>::UntilMatchView(Range&& base, Pattern pattern)
        : _current{ rg::begin(base) }, _view{ base }, _pattern{ pattern } {
        _history.reserve(rg::size(_pattern));
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    void UntilMatchView<Range, Pattern, Inclusive>::Iterator::operator++(int) { ++*this; }


    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    typename UntilMatchView<Range, Pattern, Inclusive>::Iterator UntilMatchView<Range, Pattern, Inclusive>::begin() {
        return Iterator{ this };
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    std::default_sentinel_t UntilMatchView<Range, Pattern, Inclusive>::end() {
        return {};
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
    requires ComparableRange<Range, Pattern>
    std::size_t UntilMatchView<Range, Pattern, Inclusive>::Iterator::GetIndex(std::size_t i) const noexcept {
        return i % _view->_history.size();
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    std::size_t UntilMatchView<Range, Pattern, Inclusive>::Iterator::GetTailIndex() const noexcept {
        return GetIndex(_view->_tail);
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    std::size_t UntilMatchView<Range, Pattern, Inclusive>::Iterator::GetHeadIndex() const noexcept {
        return GetIndex(_view->_head);
    }


    template<bool Inclusive, rg::contiguous_range Pattern>
    UntilMatchAdaptor<Inclusive, Pattern>::UntilMatchAdaptor(Pattern p) : pattern(std::move(p)) { }

    template<bool Inclusive, rg::contiguous_range Pattern>
    auto UntilMatch(Pattern&& pattern) {
        return UntilMatchAdaptor<Inclusive, vs::all_t<Pattern>>(vs::all(std::forward<Pattern>(pattern)));
    }

    template<rg::contiguous_range Pattern>
    auto InclusiveUntilMatch(Pattern &&pattern) {
        return UntilMatchAdaptor<true, vs::all_t<Pattern>>(vs::all(std::forward<Pattern>(pattern)));
    }

    template<rg::contiguous_range Pattern>
    auto ExclusiveUntilMatch(Pattern &&pattern) {
        return UntilMatchAdaptor<false, vs::all_t<Pattern>>(vs::all(std::forward<Pattern>(pattern)));
    }

    template<rg::viewable_range Range, rg::contiguous_range Pattern, bool Inclusive>
    auto operator|(Range&& r, const UntilMatchAdaptor<Inclusive, Pattern> &adaptor) {
        return UntilMatchView<std::remove_cv_t<Range>, Pattern, Inclusive>(
                std::forward<Range>(r), adaptor.pattern);
    }


    template<rg::sized_range R1, rg::range R2>
        requires std::indirectly_copyable<rg::iterator_t<R2>, std::back_insert_iterator<R1>>
    R1 CopyTo(R2& view) {
        R1 res;
        auto it1{ rg::begin(res) };
        auto it2{ rg::cbegin(view) };

        const auto end1{ rg::end(res) };
        const auto end2{ rg::cend(view) };


        for (; it1 != end1 && it2 != end2; ++it1, ++it2)
            *it1 = *it2;

        return res;
    }
}