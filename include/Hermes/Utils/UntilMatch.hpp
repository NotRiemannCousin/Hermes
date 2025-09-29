#pragma once
#include <ranges>
#include <deque>

namespace Hermes::Utils {
    namespace rg = std::ranges;
    namespace vs = std::views;

    template<class F, class S>
    concept ComparableRange = std::equality_comparable_with<rg::range_reference_t<F>, rg::range_reference_t<S>>;

    template<rg::input_range Range, rg::contiguous_range Pattern>
        requires ComparableRange<Range, Pattern>
    struct UntilMatchView : std::ranges::view_interface<UntilMatchView<Range, Pattern>> {
        using Type = rg::range_value_t<Range>;

        struct Iterator {
            using difference_type  = std::ptrdiff_t;
            using value_type       = Type;

            const UntilMatchView* _view{};

            rg::iterator_t<Range> _current{};
            std::deque<Type> _history{};
            bool _matchFound{};

            explicit Iterator(const UntilMatchView* parent, rg::iterator_t<Range> _current);

            [[nodiscard]] value_type operator*() const;
            Iterator& operator++();
            void operator++(int);
            [[nodiscard]] bool operator==(std::default_sentinel_t) const;
        };

        UntilMatchView(Range base, Pattern pattern);

        Iterator begin();
        std::default_sentinel_t end() const;

    private:
        Range _view;
        Pattern _pattern;
    };

    template<rg::range Pattern>
    struct UntilMatchAdaptor {
        Pattern pattern;
        explicit UntilMatchAdaptor(Pattern p);
    };

    template<rg::range Pattern>
    auto UntilMatch(Pattern&& pattern);

    template<rg::viewable_range Range, rg::range Pattern>
    auto operator|(Range&& r, const UntilMatchAdaptor<Pattern>& adaptor);


    static_assert(rg::input_range<UntilMatchView<RawInputSocketView<char>, std::string_view>>);

    template<class R1, class R2>
    R1 CopyTo(R2 &&view);
}

#include <Hermes/Utils/UntilMatch.tpp>