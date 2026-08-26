#pragma once
#include <ranges>

namespace Hermes::Utils {
    namespace rg = std::ranges;
    namespace vs = std::views;


    //! @brief A lazy view that removes the last element of a range.
    //!
    //! @details DropLastView keeps one value ahead. The current value is yielded only after another value is found.
    //! Empty or single-element ranges produce an empty view.
    template<rg::range Range>
    struct DropLastView : std::ranges::view_interface<DropLastView<Range>> {
        //! @brief The value type yielded by the input range.
        using Type = rg::range_value_t<Range>;

        //! @brief The iterator used by DropLastView.
        struct Iterator {
            using difference_type  = std::ptrdiff_t;
            using value_type       = Type;

            DropLastView* m_view{};

            // Needed so DropLastView<Range>::m_current (`rg::iterator_t<Range> m_current{}`)
            // is well-formed when Range is itself a DropLastView (i.e. `r | dropLast | dropLast`).
            // The default-constructed Iterator is only ever a transient placeholder: the
            // enclosing DropLastView's constructor immediately overwrites m_current with a
            // real value from rg::begin(), so this instance is never dereferenced.
            Iterator() noexcept = default;
            //! @brief Creates an iterator positioned at the first retained value.
            explicit Iterator(DropLastView* parent);

            //! @brief Returns the cached value that is currently being yielded.
            [[nodiscard]] value_type operator*() const;
            //! @brief Replaces the cached value with the next value in the range.
            Iterator& operator++();
            //! @brief Advances the iterator by one value.
            void operator++(int);
            //! @brief Checks whether no retained value remains.
            [[nodiscard]] bool operator==(std::default_sentinel_t) const;
        };

        //! @brief Constructs a lazy view over a range, omitting its last value.
        //! @param base The range to adapt.
        DropLastView(Range base);

        //! @brief Returns an iterator positioned at the beginning of the view.
        Iterator begin();
        //! @brief Returns the sentinel used to mark the end of the view.
        static std::default_sentinel_t end();

    private:
        rg::iterator_t<Range> m_current{};
        Type m_val{};
        std::size_t m_index{};

        Range m_view;
    };

    //! @brief A pipe adaptor that removes the last value from a range.
    struct DropLastAdaptor {
        //! @brief Creates a DropLastView over the supplied range.
        //! @param r The range to adapt.
        template<rg::range R>
        constexpr auto operator()(R&& r) const;
    };

    //! @brief The pipe adaptor used as `range | dropLast`.
    inline constexpr DropLastAdaptor dropLast{};

    //! @brief Pipes a range into the dropLast adaptor.
    //! @param r The range whose last value should be removed.
    //! @param adaptor The dropLast adaptor.
    template<rg::range Range>
    constexpr auto operator|(Range&& r, const DropLastAdaptor& adaptor);


    static_assert(rg::viewable_range<DropLastView<std::string_view>>);

}

#include <Hermes/Utils/DropLast.tpp>