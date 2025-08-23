#pragma once
#include <array>
#include <string_view>
#include <string>
#include <variant>
#include <optional>
#include <expected>
#include <vector>
#include <format>
#include <ranges>

using std::array;
using std::string_view;
using std::to_string;
using std::string;
using std::expected;
using std::optional;
using std::vector;


namespace Hermes {
    struct IpAddress {
        enum class IpClassEnum { INVALID, IPV6, A, B, C, D, E };

        using IntType = std::uint8_t;
        using IPv4Type = array<IntType, 4>;
        using IPv6Type = array<IntType, 16>;
        using IpVariant = std::variant<std::monostate, IPv4Type, IPv6Type>;

        IpVariant data;

        //----------------------------------------------------------------------------------------------------
        // Construct
        //----------------------------------------------------------------------------------------------------

        /**
         * Constructor initializing IpAddress with a variant type.
         * @param d Variant containing either IPv4 or IPv6 address data.
         */
        explicit IpAddress(const IpVariant& d);

        /**
         * Create an empty IpAddress.
         * @return An empty IpAddress.
         */
        static IpAddress Empty();

        /**
         * Create an IpAddress from an IPv4Type array.
         * @param data An array representing an IPv4 address.
         * @return The corresponding IpAddress instance.
         */
        static IpAddress FromIPv4(const IPv4Type &data);

        /**
         * Create an IpAddress from an IPv6Type array.
         * @param data An array representing an IPv6 address.
         * @return The corresponding IpAddress instance.
         */
        static IpAddress FromIPv6(const IPv6Type &data);


        /**
         * Try to parse an IpAddress from a string.
         * @param str A string representing an IPv4 or IPv6 address.
         * @return The corresponding IpAddress instance or an empty IpAddress if parsing fails.
         */
        static std::optional<IpAddress> TryParse(const string& str);

        //----------------------------------------------------------------------------------------------------
        // Info
        //----------------------------------------------------------------------------------------------------

        /**
         * Get the class of the IP address.
         * @return The IpClassEnum representing the class of the IP.
         */
        [[nodiscard]] IpClassEnum GetClass() const;

        /**
         * Check if the IpAddress is empty.
         * @return True if the IpAddress is empty.
         */
        [[nodiscard]] bool IsEmpty() const;

        /**
         * Check if the IpAddress is an IPv4 address.
         * @return True if the IpAddress is IPv4.
         */
        [[nodiscard]] bool IsIPv4() const;

        /**
         * Check if the IpAddress is an IPv6 address.
         * @return True if the IpAddress is IPv6.
         */
        [[nodiscard]] bool IsIPv6() const;

        /**
         * Check if the IpAddress is valid (not empty, unspecified and in a valid range).
         * @return True if the IpAddress is valid.
         */
        [[nodiscard]] bool IsValid() const;

        /**
         * Check if the IpAddress is a public address.
         * @return True if the IpAddress is public.
         */
        [[nodiscard]] bool IsPublic() const;

        /**
         * Check if the IpAddress is a loopback address.
         * @return True if the IpAddress is a loopback address.
         */
        [[nodiscard]] bool IsLoopback() const;

        /**
         * Check if the IpAddress is a multicast address.
         * @return True if the IpAddress is multicast.
         */
        [[nodiscard]] bool IsMulticast() const;

        /**
         * Check if the IpAddress is unspecified.
         * @return True if the IpAddress is unspecified.
         */
        [[nodiscard]] bool IsUnspecified() const;


        /**
         *
         * Check if the IpAddress is link-local.
         * @return True if the IpAddress is link-local.
         */
        [[nodiscard]] bool IsLinkLocal() const noexcept;

        /**
         *
         * Check if the IpAddress is site-local.
         * @return True if the IpAddress is site-local.
         */
        [[nodiscard]] bool IsSiteLocal() const noexcept;
        friend struct std::formatter<IpAddress>;
    };

} // namespace Hermes



namespace std {
    // template<> struct hash<Hermes::IpAddress> {
    //     size_t operator()(Hermes::IpAddress const& ip) const noexcept {
    //         if(ip.IsEmpty())
    //             return 0;
    //
    //         return std::visit([](auto const& address) {
    //             return std::apply([](auto const&... bytes) {
    //                 size_t hash = 0;
    //                 ((hash = (hash << 1) | std::hash<uint8_t>{}(bytes)), ...);
    //                 return hash;
    //             }, address);
    //         }, ip.data);
    //     }
    // };
    // };

    template<>
    struct formatter<Hermes::IpAddress> {
        using IpAddress = Hermes::IpAddress;


        /**
         * Parse function for IpAddress.
         * @param ctx The parse context.
         * @return Iterator pointing to the end of the parsed input.
         *
         * @note If 'f' is specified, it will format the IPv6 address in full format.
         */
        constexpr auto parse(auto &ctx) {
            auto &&it = ctx.begin();
            if(it != ctx.end() && *it == 'f') ipv6Reduced = true, ++it;
            return it;
        }

        /**
         * Format function for IpAddress.
         * @param ip The IpAddress object.
         * @param ctx The format context.
         * @return Iterator pointing to the end of the formatted output.
         */
        auto format(const IpAddress &ip, std::format_context &ctx) const {
            if (ip.IsIPv4()) {
                const auto &ipv4 = std::get<IpAddress::IPv4Type>(ip.data);
                return std::format_to(ctx.out(), "{}.{}.{}.{}", ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
            }

            auto &ipv6 = std::get<IpAddress::IPv6Type>(ip.data);

            if (!ipv6Reduced)
                return std::apply(
                        [&]<typename... T0>(T0 &&...args) {
                            return std::format_to(ctx.out(), ipv6_fmt, std::forward<T0>(args)...);
                        },
                        ipv6);

            // now format in reduced format
            auto segments = ipv6
                    | views::chunk(2) | views::transform([](auto &&seg) {
                        return std::format("{:x}", static_cast<int>(seg[0]) << 8 | seg[1]);
                    }) | ranges::to<std::vector>();

            auto longestRun = 0;
            auto longestRunStart = 0;
            auto currentRun = 0;
            auto currentRunStart = 0;

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
        bool ipv6Reduced{};

        static constexpr auto ipv6_fmt_data = [] {
            array<char, 256> buffer{};
            string pattern = views::repeat("{:02x}{:02x}"sv, 8) | views::join_with(':') | ranges::to<string>();
            ranges::copy(pattern, buffer.begin());
            return buffer;
        }();

        static constexpr auto ipv6_fmt = string_view(ipv6_fmt_data.data());
    };
} // namespace std
