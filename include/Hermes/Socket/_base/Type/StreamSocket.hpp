// #pragma once
// #include <Hermes/Endpoint/_base/EndpointConcept.hpp>
// #include <Hermes/Socket/_base/DefaultTransferPolicy.hpp>
// #include <Hermes/_base/WinAPI.hpp>
//
// namespace Hermes {
//     //! @brief This class it's just a wrapper to help dealing with stream sockets.
//     //! Sockets are dumb, they don't know anything about how to know if all data was sent, received or even
//     //! if it needs to be decrypted. They just send raw data and reads raw data as it arrives. This is the
//     //! role of the protocols that inherit from StreamSocket.
//     //!
//     //! All inherited classes needs to use CRTP.
//     template<EndpointConcept EndpointType,
//             template<class> class InputSocketRange,
//             template<class> class OutputSocketRange,
//             ConnectionPolicyConcept HandleType>
//         requires SendPolicyConcept<InputSocketRange> && SocketRecvRangeConcept<OutputSocketRange>
//     struct StreamSocket {
//         // TODO: make OutputSocketRangeConcept
//
//         StreamSocket(StreamSocket&&) noexcept;
//         StreamSocket& operator=(StreamSocket&&) noexcept;
//         ~StreamSocket();
//
//         //! @brief SOCKETs are fragile. This function exposes the underlying SOCKET to the real world.
//         //!
//         //! Use with caution, this can mess up the entire class. It was designed to exposes the
//         //! SOCKET in case you need to call some specific function of handshake, decrypt, auth
//         //! or something like that.
//         SOCKET& UnsafeUnderlyingSocket();
//
//         [[nodiscard]] static ConnectionResult<T> Connect(const EndpointType& endpoint);
//         //! @return Returns the count of data sent.
//         [[nodiscard]] StreamSent SendRaw(ByteDataSpan data) const;
//
//         //! @return Returns a lazy view to the data received by the socket.
//         [[nodiscard]] InputSocketRange<std::byte> ReceiveRaw() const;
//         //! @return Same as ReceiveRaw() but returns as char instead of std::byte.
//         [[nodiscard]] InputSocketRange<char> ReceiveStr() const;
//
//         void Close() const;
//         [[nodiscard]] bool IsOpen() const;
//
//     protected:
//         StreamSocket() noexcept;
//         SOCKET _socket{ macroINVALID_SOCKET };
//     };
// } // namespace Hermes
//
// #include <Hermes/Socket/_base/Type/StreamSocket.tpp>
