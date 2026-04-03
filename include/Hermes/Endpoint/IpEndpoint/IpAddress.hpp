#pragma once
#include <Hermes/Utils/Hash.hpp>

#include <array>
#include <string>
#include <variant>
#include <optional>
#include <vector>
#include <format>

namespace Hermes {
    struct IpAddress {
        using Ipv4Type = std::array<std::uint8_t, 4>;
        using Ipv6Type = std::array<std::uint8_t, 16>;
        using IpVariant = std::variant<Ipv4Type, Ipv6Type>;

        //----------------------------------------------------------------------------------------------------
        // Construct
        //----------------------------------------------------------------------------------------------------

        IpAddress() noexcept = default;

        //!  Constructor initializing IpAddress with a variant type.
        //!  @param d Variant containing either IPv4 or IPv6 address data.
        explicit IpAddress(const IpVariant& d);


        IpAddress(const IpAddress&) noexcept = default;
        IpAddress& operator=(const IpAddress&) noexcept = default;


        //! Create an IpAddress from an Ipv4Type array.
        //! @param data An array representing an IPv4 address.
        //! @return The corresponding IpAddress instance.
        static IpAddress FromIpv4(const Ipv4Type &data);

        //! Create an IpAddress from an Ipv6Type array.
        //! @param data An array representing an IPv6 address.
        //! @return The corresponding IpAddress instance.
        static IpAddress FromIpv6(const Ipv6Type &data);



        //! Try to parse an IpAddress from a string.
        //! @param str A string representing an IPv4 or IPv6 address.
        //! @return The corresponding IpAddress instance or an empty IpAddress if parsing fails.
        static std::optional<IpAddress> TryParse(const std::string& str);


        //! Get the IpVariant value.
        [[nodiscard]] IpVariant GetIp() const;

        //----------------------------------------------------------------------------------------------------
        // Info
        //----------------------------------------------------------------------------------------------------


        std::strong_ordering operator<=>(const IpAddress &) const = default;


        //! Check if the IpAddress is an IPv4 address.
        //! @return True if the IpAddress is IPv4.
        [[nodiscard]] bool IsIpv4() const;

        //!  Check if the IpAddress is an IPv6 address.
        //!  @return True if the IpAddress is IPv6.
        [[nodiscard]] bool IsIpv6() const;




        //! Check if the IpAddress is valid (not empty, unspecified and in a valid range).
        //! @return True if the IpAddress is valid.
        [[nodiscard]] bool IsRoutable() const;

        //! Check if the IpAddress is a public address.
        //! @return True if the IpAddress is public.
        [[nodiscard]] bool IsPublic() const;

        //! Check if the IpAddress is a private address.
        //! @return True if the IpAddress is private.
        [[nodiscard]] bool IsPrivate() const;




        //! Check if the IpAddress is a loopback address.
        //! @return True if the IpAddress is a loopback address.
        [[nodiscard]] bool IsLoopback() const;

        //! Check if the IpAddress is a multicast address.
        //! @return True if the IpAddress is multicast.
        [[nodiscard]] bool IsMulticast() const;

        //! Check if the IpAddress is unspecified.
        //! @return True if the IpAddress is unspecified.
        [[nodiscard]] bool IsUnspecified() const;




        //! Check if the IpAddress is link-local.
        //! @return True if the IpAddress is link-local.
        [[nodiscard]] bool IsLinkLocal() const noexcept;

        //! Check if the IpAddress is site-local.
        //! @return True if the IpAddress is site-local.
        [[nodiscard]] bool IsSiteLocal() const noexcept;



        //! Check if the IPv6 address is an IPv4-mapped address (::ffff:0:0/96).
        //! @return True if the IpAddress is IPv4-mapped.
        [[nodiscard]] bool IsIpv4Mapped() const noexcept;

        //! Check if the IPv6 address is a documentation address (2001:db8::/32).
        //! @return True if the IpAddress is documentation.
        [[nodiscard]] bool IsDocumentation() const noexcept;

        friend struct std::formatter<IpAddress>;
        friend struct std::hash<IpAddress>;

    private:
        IpVariant _data{};
    };

}

#include <Hermes/Endpoint/IpEndpoint/IpAddress.tpp>

static_assert(std::formattable<Hermes::IpAddress, char>);
static_assert(Hermes::Utils::Hashable<Hermes::IpAddress>);