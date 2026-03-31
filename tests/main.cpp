#include <BaseTests.hpp>

#include "Hermes/Endpoint/IpEndpoint/IpAddress.hpp"


int main() {
    // IpAddressTests();
    // IpEndpointTests();

    std::println("{}", sizeof(std::optional<Hermes::IpAddress>));
    std::println("{}", sizeof(Hermes::IpAddress));

    // TcpTests();

    return 0;
}
