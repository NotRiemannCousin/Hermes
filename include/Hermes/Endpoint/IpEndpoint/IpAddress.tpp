#pragma once
#include <Hermes/Utils/Hash.hpp>
#include <format>
#include <ranges>

namespace std {
    template<>
    struct hash<Hermes::IpAddress> {
        size_t operator()(const Hermes::IpAddress& ip) const noexcept {
            return std::visit([]<class T>(const T& address) {
                return std::apply([](const auto&... bytes) {
                    size_t hash{};
                    ((hash = hash << 1 | std::hash<uint8_t>{}(bytes)), ...);
                    return hash;
                }, address);
            }, ip._data);
        }
    };

    template<>
    struct formatter<Hermes::IpAddress> {
        using IpAddress = Hermes::IpAddress;

        //! Parse function for IpAddress.
        //! @param ctx The parse context.
        //! @return Iterator pointing to the end of the parsed input.
        //!
        //! @note If 'f' is specified, IPv6 will be formatted in the full format.
        //! @note If 'b' is specified, IPv6 will be formatted between '[' and ']'.
        //! @note The order of the args matters, 'f' needs to be before 'b'.
        constexpr auto parse(auto &ctx) {
            auto &&it = ctx.begin();
            if(it != ctx.end() && *it == 'f') _ipv6Reduced  = true, ++it;
            if(it != ctx.end() && *it == 'b') _ipv6Brackets = true, ++it;
            return it;
        }

        //! Format function for IpAddress.
        //! @param ip The IpAddress object.
        //! @param ctx The format context.
        //! @return Iterator pointing to the end of the formatted output.
        template<class FormatContext>
        auto format(const IpAddress &ip, FormatContext &ctx) const {
            if (ip.IsIpv4()) {
                const auto &ipv4 = std::get<IpAddress::Ipv4Type>(ip._data);
                return std::format_to(ctx.out(), "{}.{}.{}.{}", ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
            }

            auto &ipv6 = std::get<IpAddress::Ipv6Type>(ip._data);

            if (!_ipv6Reduced)
                return std::apply(
                        [&]<class... T0>(T0 &&...args) {
                            return std::format_to(ctx.out(), ipv6Fmt, std::forward<T0>(args)...);
                        },
                        ipv6);

            auto segments{ ipv6
                    | views::chunk(2) | views::transform([](auto &&seg) {
                        return std::format("{:x}", static_cast<int>(seg[0]) << 8 | seg[1]);
                    }) | ranges::to<std::vector>() };

            std::size_t longestRun{};
            std::size_t longestRunStart{};
            std::size_t currentRun{};
            std::size_t currentRunStart{};

            for (auto &&seg : segments) {
                if (seg == "0") {
                    if (currentRun == 0)
                        currentRunStart = longestRunStart = &seg - segments.data();
                    currentRun++;
                } else {
                    if (currentRun > longestRun) {
                        longestRun = currentRun;
                        longestRunStart = currentRunStart;
                    }
                    currentRun = 0;
                }
            }

            if (longestRun > 1) {
                segments[longestRunStart] = (longestRunStart == 0 || longestRunStart + longestRun == segments.size()) ? ":" : "";
                segments.erase(segments.begin() + longestRunStart + 1, segments.begin() + longestRunStart + longestRun);
            }


                return std::format_to(ctx.out(), "{}", views::join_with(segments, ':') | ranges::to<string>());
        }

    private:
        bool _ipv6Reduced{};
        bool _ipv6Brackets{};

        static constexpr auto ipv6FmtData = [] {
            array<char, 256> buffer{};
            string pattern = views::repeat("{:02x}{:02x}"sv, 8) | views::join_with(':') | ranges::to<string>();
            ranges::copy(pattern, buffer.begin());
            return buffer;
        }();

        static constexpr auto ipv6Fmt = string_view{ ipv6FmtData.data() };
    };
}

