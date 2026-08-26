#pragma once
#include <ranges>
#include <vector>

namespace Hermes::Utils {
    namespace rg = std::ranges;
    namespace vs = std::views;

    //! @brief Checks whether two ranges expose equality-comparable references.
    template<class F, class S>
    concept ComparableRange = std::equality_comparable_with<rg::range_reference_t<F>, rg::range_reference_t<S>>;

    //! @brief A range whose end is determined by the first occurrence of a pattern.
    //! @details UntilMatchView consumes Range lazily and stops when Pattern is found.
    //! When Inclusive is false, the matching pattern is excluded. When true, it is included before the view ends.
    //! The view keeps a history buffer to detect a match without requiring Range to be multi-pass.
    template<rg::input_range Range, rg::contiguous_range Pattern, bool Inclusive>
        requires ComparableRange<Range, Pattern>
    struct UntilMatchView : std::ranges::view_interface<UntilMatchView<Range, Pattern, Inclusive>> { 
        //! @brief The value type yielded by the input range.
        using Type = rg::range_value_t<Range>;

        //! @brief The iterator used to scan the input and detect the pattern.
        struct Iterator {
            using difference_type  = std::ptrdiff_t;
            using value_type       = Type;

            UntilMatchView* m_view{};

            explicit Iterator(UntilMatchView* parent);

            [[nodiscard]] value_type operator*() const;
            Iterator& operator++();
            void operator++(int);
            [[nodiscard]] bool operator==(std::default_sentinel_t) const;

            //! @brief Maps an absolute history position to the circular buffer.
            std::size_t GetIndex(std::size_t i) const noexcept;
            //! @brief Returns the history position currently being yielded.
            std::size_t GetTailIndex() const noexcept;
            //! @brief Returns the history position where the next value is stored.
            std::size_t GetHeadIndex() const noexcept;
        };

        //! @brief Constructs a lazy view over a range and a contiguous match pattern.
        //! @param base The input range to scan.
        //! @param pattern The sequence that terminates the view.
        UntilMatchView(Range&& base, Pattern pattern);

        //! @brief Returns an iterator positioned at the beginning of the view.
        Iterator begin();
        //! @brief Returns the sentinel used to mark the end of the view.
        static std::default_sentinel_t end();

    private:

        rg::iterator_t<Range> m_current{};
        std::vector<Type> m_history{};
        std::size_t m_head{};
        std::size_t m_tail{};
        bool m_matchFound{};

        Range m_view;
        Pattern m_pattern;
    };

    //! @brief A pipe adaptor that stores the pattern used by UntilMatchView.
    template<bool Inclusive, rg::contiguous_range Pattern>
    struct UntilMatchAdaptor {
        Pattern pattern;
        //! @brief Stores a copy of the termination pattern.
        explicit UntilMatchAdaptor(Pattern p);
    };

    //! @brief Creates an adaptor that stops at the pattern.
    //! @tparam Inclusive Whether the matching pattern is included in the output.
    //! @param pattern The contiguous sequence to search for.
    template<bool Inclusive = false, rg::contiguous_range Pattern>
    auto UntilMatch(Pattern&& pattern);

    //! @brief Creates an adaptor that includes the first matching pattern.
    //! @param pattern The contiguous sequence to search for.
    template<rg::contiguous_range Pattern>
    auto InclusiveUntilMatch(Pattern&& pattern);

    //! @brief Creates an adaptor that excludes the first matching pattern.
    //! @param pattern The contiguous sequence to search for.
    template<rg::contiguous_range Pattern>
    auto ExclusiveUntilMatch(Pattern&& pattern);

    //! @brief Pipes a range into an UntilMatch adaptor.
    //! @param r The viewable input range to scan.
    //! @param adaptor The stored termination pattern and inclusion mode.
    template<rg::viewable_range Range, rg::contiguous_range Pattern, bool Inclusive>
    auto operator|(Range&& r, const UntilMatchAdaptor<Inclusive, Pattern>& adaptor);

    //! @brief Copies a range into a new container of the requested type.
    //! @param view The range to extract.
    //! @return A container populated with the values from view.
    //! @details If the destination supports push_back(), all values are copied. Otherwise the destination's existing
    //! size limits the number of copied values.
    template<rg::sized_range R1, rg::range R2>
    requires std::default_initializable<R1> && (
    std::indirectly_copyable<rg::iterator_t<R2>, rg::iterator_t<R1>>
    || requires(R1& r, rg::range_reference_t<R2> val) { r.push_back(val); }
    && std::indirectly_copyable<rg::iterator_t<R2>, std::back_insert_iterator<R1>>
)
    R1 ExtractTo(R2& view);
}

#include <Hermes/Utils/UntilMatch.tpp>