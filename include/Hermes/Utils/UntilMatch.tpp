#pragma once

namespace Hermes::Utils {
    template<rg::input_range Range, rg::contiguous_range Pattern> requires ComparableRange<Range, Pattern>
    UntilMatchView<Range, Pattern>::Iterator::Iterator(const UntilMatchView *parent, rg::iterator_t<Range> current)
        : _view(parent), _current(current) { }

    template<rg::input_range Range, rg::contiguous_range Pattern>
        requires ComparableRange<Range, Pattern>
    typename UntilMatchView<Range, Pattern>::Type UntilMatchView<Range, Pattern>::Iterator::operator*() const {
        return *_current;
    }

    template<rg::input_range Range, rg::contiguous_range Pattern>
        requires ComparableRange<Range, Pattern>
    typename UntilMatchView<Range, Pattern>::Iterator& UntilMatchView<Range, Pattern>::Iterator::operator++() {
        _history.push_back(*_current);

        if (_history.size() > rg::size(_view->_pattern)) {
            _history.pop_front();
        }


        if (_history.size() == rg::size(_view->_pattern))
            if (rg::equal(_history, _view->_pattern))
                _matchFound = true;

        ++(_current);
        return *this;
    }

    template<rg::input_range Range, rg::contiguous_range Pattern>
        requires ComparableRange<Range, Pattern>
    void UntilMatchView<Range, Pattern>::Iterator::operator++(int) { ++(*this); }

    template<rg::input_range Range, rg::contiguous_range Pattern>
        requires ComparableRange<Range, Pattern>
    bool UntilMatchView<Range, Pattern>::Iterator::operator==(std::default_sentinel_t) const {
        return _matchFound;
    }



    template<rg::input_range Range, rg::contiguous_range Pattern>
        requires ComparableRange<Range, Pattern>
    UntilMatchView<Range, Pattern>::UntilMatchView(Range base, Pattern pattern)
        : _view(std::move(base)),
          _pattern(pattern) {
    }

    template<rg::input_range Range, rg::contiguous_range Pattern>
        requires ComparableRange<Range, Pattern>
    typename UntilMatchView<Range, Pattern>::Iterator UntilMatchView<Range, Pattern>::begin() {
        return Iterator{ this, rg::begin(_view) };
    }

    template<rg::input_range Range, rg::contiguous_range Pattern>
        requires ComparableRange<Range, Pattern>
    std::default_sentinel_t UntilMatchView<Range, Pattern>::end() const {
        return {};
    }



    template<rg::range Pattern>
    UntilMatchAdaptor<Pattern>::UntilMatchAdaptor(Pattern p) : pattern(std::move(p)) { }

    template<rg::range Pattern>
    auto UntilMatch(Pattern &&pattern) {
        return UntilMatchAdaptor<vs::all_t<Pattern>>(vs::all(std::forward<Pattern>(pattern)));
    }

    template<rg::viewable_range Range, rg::range Pattern>
    auto operator|(Range &&r, const UntilMatchAdaptor<Pattern> &adaptor) {
        return UntilMatchView(vs::all(std::forward<Range>(r)), adaptor.pattern);
    }


    template<rg::range R1, rg::range R2>
        requires std::indirectly_copyable<rg::iterator_t<R2>, std::back_insert_iterator<R1>>
    R1 CopyTo(const R2 &view) {
        R1 res;

        rg::copy_result(view, res);

        return std::move(res);
    }
}