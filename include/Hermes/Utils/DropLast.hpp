#pragma once
#include <ranges>

namespace Hermes::Utils {
    namespace rg = std::ranges;
    namespace vs = std::views;


    template<rg::range Range>
    struct DropLastView : std::ranges::view_interface<DropLastView<Range>> {
        using Type = rg::range_value_t<Range>;

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
            explicit Iterator(DropLastView* parent);

            [[nodiscard]] value_type operator*() const;
            Iterator& operator++();
            void operator++(int);
            [[nodiscard]] bool operator==(std::default_sentinel_t) const;
        };

        DropLastView(Range base);

        Iterator begin();
        static std::default_sentinel_t end();

    private:
        rg::iterator_t<Range> m_current{};
        Type m_val{};
        std::size_t m_index{};

        Range m_view;
    };

    struct DropLastAdaptor {
        template<rg::range R>
        constexpr auto operator()(R&& r) const;
    };

    inline constexpr DropLastAdaptor dropLast{};

    template<rg::range Range>
    constexpr auto operator|(Range&& r, const DropLastAdaptor& adaptor);


    static_assert(rg::viewable_range<DropLastView<std::string_view>>);

}

#include <Hermes/Utils/DropLast.tpp>