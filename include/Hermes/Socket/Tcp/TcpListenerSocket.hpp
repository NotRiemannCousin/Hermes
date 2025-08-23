// #pragma once
//
// #include <Hermes/Socket/ClientSocket.hpp>
// #include <functional>
//
// namespace Hermes {
//     //! @brief A stream-based server listener, specialized for TCP/IP protocols.
//     //!
//     //! This class is used on the server side to bind to a local network interface and listen for
//     //! incoming client connections. It does not handle data transfer itself; instead, its purpose is to
//     //! accept connections and produce connected ClientSocket objects.
//     //!
//     //! The typical lifecycle involves:
//     //! 1. Creating an instance via the static @ref Bind() method.
//     //! 2. Calling @ref Listen() to start waiting for connections.
//     //! 3. Calling @ref Accept() in a loop to handle each incoming connection.
//     //!
//     //! @see BaseSocket
//     //! @see ClientSocket
//     //!
//     //! @code
//     //! #include <iostream>
//     //! #include <thread>
//     //! #include <print>
//     //!
//     //! using std::println;
//     //!
//     //! void HandleConnection(Hermes::ClientSocket client) {
//     //!     println("Accepted a new connection.");
//     //!     // Handle client communication...
//     //!     client.Close();
//     //! }
//     //!
//     //! void RunServer() {
//     //!     // Bind and listen in a local address and port.
//     //!     auto endpoint{ Hermes::Endpoint::FromIpv4({0, 0, 0, 0}, 80) };
//     //!
//     //!     auto listenerResult{ Hermes::ListenerSocket::BindAndListen(endpoint, 8080) };
//     //!
//     //!     if (!listenerResult) {
//     //!         println(std::cerr, "Failed to listen {} with error code {}", endpoint, (size_t)listenerResult.error());
//     //!         return;
//     //!     }
//     //!
//     //!     // Loop forever, accepting new clients.
//     //!     while (listener.IsOpen()) {
//     //!         auto clientResult{ listener.Accept() };
//     //!         if (clientResult) {
//     //!             // Spawn a new thread to handle the client.
//     //!             std::thread(HandleConnection, std::move(*clientResult)).detach();
//     //!         } else {
//     //!             // An error occurred in Accept(), maybe the listener was closed.
//     //!             println(std::cerr, "Failed to accept connection: {}", (size_t)clientResult.error());
//     //!             break;
//     //!         }
//     //!     }
//     //! }
//     //! @endcode
//     struct ListenerSocket : BaseSocket {
//         //! @brief Binds and put the socket to listen to a local network address and port.
//         //!
//         //! This factory method creates a socket and associates it with a specific local IP address and port. If it
//         //! success, put the socket to listen for incoming connections.
//         //!
//         //! @param address The endpoint (local IP address and port) to bind to. Use "0.0.0.0" to listen on all available IPv4 interfaces
//         //! or "::" for all available IPv6 interfaces.
//         //! @param backlog The maximum length of the queue for pending connections.
//         //! @return An std::expected containing a ListenerSocket ready to listen on success,
//         //! or a ConnectionErrorEnum on failure (e.g., address in use, invalid address).
//         [[nodiscard]] static std::expected<ListenerSocket, ConnectionErrorEnum> BindAndListen(Endpoint address, int backlog = 5);
//
//         //! @brief Accepts an incoming client connection.
//         //!
//         //! Waits until a client attempts to connect to the listener's address and port (blocking operation).
//         //! When a connection is established, it returns a new, fully operational ClientSocket for communication
//         //! with that specific client.
//         //!
//         //! @return An std::expected containing a new ClientSocket for the accepted connection on success,
//         //! or a ConnectionErrorEnum on failure.
//         [[nodiscard]] ConnectionResult<ClientSocket> Accept();
//
//         //! @brief Accepts an incoming client connection.
//         //!
//         //! When a client attempts to connect to the listener's address and port, triggers the callback with the accepted
//         //! socket (non-blocking operation).
//         //!
//         //! @return An std::expected containing a new @ref ClientSocket for the accepted connection on success,
//         //! or a ConnectionErrorEnum on failure.
//         void AsyncAccept(std::function<void(ClientSocket&)> callback);
//     };
// }