#pragma once
#include <algorithm>

namespace Hermes::Utils {
    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    UntilMatchView<Range, Pattern, Inclusive>::Iterator::Iterator(UntilMatchView *parent)
        : _view(parent) {
        if constexpr (!Inclusive)
            for (const char c : _view->_pattern)
                _view->_history.push_back(*_view->_current), ++_view->_current;
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    typename UntilMatchView<Range, Pattern, Inclusive>::Type UntilMatchView<Range, Pattern, Inclusive>::Iterator::operator*() const {
        if constexpr (Inclusive)
            return *_view->_current;
        else
            return _view->_history.front();
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    typename UntilMatchView<Range, Pattern, Inclusive>::Iterator&
    UntilMatchView<Range, Pattern, Inclusive>::Iterator::operator++() {
        if (_view->_matchFound) return *this;

        _view->_history.push_back(*_view->_current);
        ++_view->_current;

        if (_view->_history.size() > rg::size(_view->_pattern)) {
            _view->_history.pop_front();
        }

        if (_view->_history.size() == rg::size(_view->_pattern)) {
            if (rg::equal(_view->_history, _view->_pattern)) {
                _view->_matchFound = true;
            }
        }

        return *this;
    }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    void UntilMatchView<Range, Pattern, Inclusive>::Iterator::operator++(int) { ++(*this); }

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    bool UntilMatchView<Range, Pattern, Inclusive>::Iterator::operator==(std::default_sentinel_t) const {
        return _view->_matchFound;
    }



    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    UntilMatchView<Range, Pattern, Inclusive>::UntilMatchView(Range&& base, Pattern pattern)
        : _view(base), _pattern(pattern), _current{ rg::begin(base) } { }

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




    template<rg::range Pattern, bool Inclusive>
    UntilMatchAdaptor<Pattern, Inclusive>::UntilMatchAdaptor(Pattern p) : pattern(std::move(p)) { }

    template<rg::range Pattern, bool Inclusive>
    auto UntilMatch(Pattern&& pattern) {
        return UntilMatchAdaptor<vs::all_t<Pattern>, Inclusive>(vs::all(std::forward<Pattern>(pattern)));
    }

    template<rg::viewable_range Range, rg::range Pattern, bool Inclusive>
    auto operator|(Range&& r, const UntilMatchAdaptor<Pattern, Inclusive> &adaptor) {
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