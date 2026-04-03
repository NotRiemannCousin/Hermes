#pragma once
#include <Hermes/Utils/Hash.hpp>
#include <format>
#include <functional>
#include <ranges>

#include <Hermes/Utils/Overloads.hpp>
#include <Hermes/Utils/Hash.hpp>

namespace std {
    template<>
    struct hash<Hermes::IpAddress> {
        size_t operator()(const Hermes::IpAddress ip) const noexcept {
            return std::visit([](const auto data) {
                size_t result{};

                for (const auto seg : data)
                    Hermes::Utils::HashCombine(result, seg);

                return result;
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
            const auto s_formatIpv6 = [&]<auto Fmt>() {
                return [&](auto... args) {
                    auto out{ ctx.out() };

                    if (_ipv6Brackets) *out++ = '[';
                    out = std::format_to(out, Fmt, args...);
                    if (_ipv6Brackets) *out++ = ']';

                    return out;
                };
            };

            std::visit(Hermes::Utils::Overloaded{
                [&](IpAddress::Ipv4Type ipv4) {
                    return std::format_to(ctx.out(), "{}.{}.{}.{}", ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
                },
                [&](IpAddress::Ipv6Type ipv6) {
                    if (!_ipv6Reduced)
                        return std::apply(s_formatIpv6.template operator()<ipv6Fmt.data()>(), ipv6);

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
                                currentRunStart = &seg - segments.data();
                            currentRun++;
                        } else {
                            if (currentRun > longestRun) {
                                longestRun = currentRun;
                                longestRunStart = currentRunStart;
                            }
                            currentRun = 0;
                        }
                    }

                    if (currentRun > longestRun) {
                        longestRun = currentRun;
                        longestRunStart = currentRunStart;
                    }

                    if (longestRun > 1) {
                        segments[longestRunStart] = (longestRunStart == 0 || longestRunStart + longestRun == segments.size()) ? ":" : "";
                        segments.erase(segments.begin() + longestRunStart + 1, segments.begin() + longestRunStart + longestRun);
                    }

                    constexpr auto fmt{ "{:s}" };
                    return s_formatIpv6.template operator()<fmt>()(views::join_with(segments, ':'));
                }
            }, ip._data);
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

