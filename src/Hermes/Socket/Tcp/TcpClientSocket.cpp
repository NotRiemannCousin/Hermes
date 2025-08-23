#include <Hermes/Socket/Tcp/TcpClientSocket.hpp>

namespace Hermes {
    // void S_TryInitialize(SOCKET _socket, const Endpoint& endpoint, SocketTypeEnum type) {
    //     auto protocol = static_cast<int>(type == SocketTypeEnum::STREAM ? ProtocolBaseTypeEnum::TCP : ProtocolBaseTypeEnum::UDP);
    //
    //     int addressFamily = _tl(endpoint.ip.IsIPv6() ? AddressFamilyEnum::INET6 : AddressFamilyEnum::INET);
    //
    //     _socket = socket(addressFamily, static_cast<int>(type), protocol);
    // }
    //
    //
    //
    //
    // template<class Socket, SocketProtocol Protocol> requires(std::derived_from<Socket, BaseSocket>)
    // std::optional<BaseSocket> BaseSocket::FromRawBaseSocket(SOCKET socket) { // NOLINT(readability-convert-member-functions-to-static)
    //     if (socket == macroINVALID_SOCKET)
    //         return std::nullopt;
    //
    //     // Get protocol info
    //     WSAPROTOCOL_INFO info{};
    //     int len = sizeof(info);
    //     if (getsockopt(socket, macroSOL_SOCKET, macroSO_PROTOCOL_INFO, reinterpret_cast<char *>(&info), &len) ==
    //         macroSOCKET_ERROR) {
    //         closesocket(socket);
    //         return std::nullopt;
    //     }
    //
    //     IpAddress localIp{IpAddress::Empty()}, remoteIp{IpAddress::Empty()};
    //     int localPort{}, remotePort{};
    //
    //     // Get local address
    //     sockaddr_storage localAddr{};
    //     int localLen = sizeof(localAddr);
    //     if (getsockname(socket, reinterpret_cast<sockaddr *>(&localAddr), &localLen) == 0) {
    //         if (localAddr.ss_family == AddressFamilyEnum::INET) {
    //             auto *in = reinterpret_cast<sockaddr_in *>(&localAddr);
    //             localIp = IpAddress::FromIPv4(std::bit_cast<IpAddress::IPv4Type>(in->sin_addr));
    //             localPort = ntohs(in->sin_port);
    //         } else if (localAddr.ss_family == AddressFamilyEnum::INET6) {
    //             auto *in6 = reinterpret_cast<sockaddr_in6 *>(&localAddr);
    //             localIp = IpAddress::FromIPv6(std::bit_cast<IpAddress::IPv6Type>(in6->sin6_addr));
    //             localPort = ntohs(in6->sin6_port);
    //         }
    //     }
    //
    //     // Get remote address
    //     sockaddr_storage remoteAddr{};
    //     int remoteLen = sizeof(remoteAddr);
    //     if (getpeername(socket, reinterpret_cast<sockaddr *>(&remoteAddr), &remoteLen) == 0) {
    //         if (remoteAddr.ss_family == AddressFamilyEnum::INET) {
    //             auto *in = reinterpret_cast<sockaddr_in *>(&remoteAddr);
    //             remoteIp = IpAddress::FromIPv4(std::bit_cast<IpAddress::IPv4Type>(in->sin_addr));
    //             remotePort = ntohs(in->sin_port);
    //         } else if (remoteAddr.ss_family == AddressFamilyEnum::INET6) {
    //             auto *in6 = reinterpret_cast<sockaddr_in6 *>(&remoteAddr);
    //             remoteIp = IpAddress::FromIPv6(std::bit_cast<IpAddress::IPv6Type>(in6->sin6_addr));
    //             remotePort = ntohs(in6->sin6_port);
    //         }
    //     }
    //
    //     auto local = Endpoint(localIp, localPort);
    //     auto remote = Endpoint(remoteIp, remotePort);
    //     BaseSocket s(std::make_unique<Protocol>(), socket, local, remote);
    //
    //     return s;
    // }
    //
    //
    // std::expected<ConnectionSocket, ConnectionErrorEnum> ConnectionSocket::Connect(const Endpoint &remote) {
    //     if (remote.ip.IsEmpty() || remote.port == -1)
    //         return unexpected{ConnectionErrorEnum::INVALID_ENDPOINT};
    //
    //     ConnectionSocket s(Endpoint(), remote);
    //     s.TryInitialize();
    //
    //     auto &&[addr, len] = S_SolveEndpoint(remote);
    //
    //     if (connect(s._socket, reinterpret_cast<sockaddr *>(&addr), len) == macroSOCKET_ERROR) {
    //         s.Cleanup();
    //         return unexpected{ConnectionErrorEnum::CONNECTION_FAILED};
    //     }
    //
    //
    //     return s;
    // }
    //
    // template<SocketProtocol Protocol>
    // ConnectionResult<ConnectionSocket> ListenerSocket::Accept() {
    //     if (_socket == macroINVALID_SOCKET)
    //         return std::unexpected{ConnectionErrorEnum::SOCKET_NOT_OPEN};
    //
    //     sockaddr_storage addr{};
    //     socklen_t len = sizeof(addr);
    //     SOCKET accepted = accept(_socket, reinterpret_cast<sockaddr *>(&addr), &len);
    //
    //     if (accepted == macroINVALID_SOCKET)
    //         return std::unexpected{ConnectionErrorEnum::UNKNOWN};
    //
    //     auto opt = FromRawBaseSocket<Protocol>(accepted);
    //     if (!opt) {
    //         closesocket(accepted);
    //         return std::unexpected{ConnectionErrorEnum::UNKNOWN};
    //     }
    //
    //     return std::move(*opt);
    // }
    //
    // ConnectionResult<TcpClientSocket> ClientSocket::Connect(Endpoint endpoint) {
    //
    // }


}
