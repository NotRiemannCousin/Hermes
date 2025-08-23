#pragma once
#include <Hermes/Socket/_base/StreamSocket.hpp>
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>

namespace Hermes {

    struct TcpClientSocket : StreamSocket<IpEndpoint, TcpClientSocket>{ };
} // namespace Hermes