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

            DropLastView* _view{};

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
        rg::iterator_t<Range> _current{};
        Type _val{};
        std::size_t _index{};

        Range _view;
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