#pragma once
#include <Hermes/Socket/_base/Type/StreamSocket.hpp>
#include <Hermes/Socket/_base/DefaultConnectPolicy.hpp>
#include <Hermes/Socket/_base/DefaultTransferPolicy.hpp>
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>

namespace Hermes {
    //! A stream socket implementation for a TCP client.
    //!
    //! This class represents the client-side of a TCP connection, using IpEndpoint
    //! for addressing.

    // struct TcpClientSocket : StreamSocket<IpEndpoint, RawInputSocketRange, DefaultSendRange, TcpClientSocket>{ };
} // namespace Hermes