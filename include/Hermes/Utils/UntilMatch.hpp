#pragma once
#include <ranges>
#include <deque>

namespace Hermes::Utils {
    namespace rg = std::ranges;
    namespace vs = std::views;

    template<class F, class S>
    concept ComparableRange = std::equality_comparable_with<rg::range_reference_t<F>, rg::range_reference_t<S>>;

    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    struct UntilMatchView : std::ranges::view_interface<UntilMatchView<Range, Pattern, Inclusive>> {
        using Type = rg::range_value_t<Range>;

        struct Iterator {
            using difference_type  = std::ptrdiff_t;
            using value_type       = Type;

            UntilMatchView* _view{};

            explicit Iterator(UntilMatchView* parent);

            [[nodiscard]] value_type operator*() const;
            Iterator& operator++();
            void operator++(int);
            [[nodiscard]] bool operator==(std::default_sentinel_t) const;
        };

        UntilMatchView(Range&& base, Pattern pattern);

        Iterator begin();
        static std::default_sentinel_t end();

    private:
        rg::iterator_t<Range> _current{};
        std::deque<Type> _history{};
        bool _matchFound{};

        Range& _view;
        Pattern _pattern;
    };

    template<rg::range Pattern, bool Inclusive>
    struct UntilMatchAdaptor {
        Pattern pattern;
        explicit UntilMatchAdaptor(Pattern p);
    };

    template<rg::range Pattern, bool Inclusive = false>
    auto UntilMatch(Pattern&& pattern);

    template<rg::viewable_range Range, rg::range Pattern, bool Inclusive>
    auto operator|(Range&& r, const UntilMatchAdaptor<Pattern, Inclusive>& adaptor);


    // TODO: static_assert(rg::input_range<UntilMatchView<RawInputSocketRange<char>, std::string_view>>);

    template<rg::sized_range R1, rg::range R2>
        requires std::indirectly_copyable<rg::iterator_t<R2>, std::back_insert_iterator<R1>>
    R1 CopyTo(R2& view);
}

#include <Hermes/Utils/UntilMatch.tpp>