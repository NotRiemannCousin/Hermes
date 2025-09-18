#include "IpTests.hpp"
#include "TcpTests.hpp"


int main() {
    IpAddressTests();
    IpEndpointTests();

    TcpTests();

    return 0;
}