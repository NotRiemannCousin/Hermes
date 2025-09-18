#pragma once
#include <Hermes/Endpoint/_base/EndpointConcept.hpp>
#include <Hermes/_base/WinAPI.hpp>

namespace Hermes {
    //! @brief This class it's just a wrapper to help dealing with dgram sockets.
    //! Sockets are dumb, they don't know anything about how to know if all data was sent, received or even
    //! if it needs to be decrypted. They just send raw data and reads raw data as it arrives. This is the
    //! role of the protocols that inherit from DgramSocket.
    //!
    //! All inherited classes needs to use CRTP.
    template<EndpointConcept EndpointType>
    struct DgramSocket {

        DgramSocket() noexcept;
        DgramSocket(DgramSocket&&) noexcept;
        DgramSocket& operator=(DgramSocket&&) noexcept;
        ~DgramSocket();

        //! @brief SOCKETs are fragile. This function exposes the underlying SOCKET to the real world.
        //!
        //! Use with caution, this can mess up the entire class. It was designed to exposes the
        //! SOCKET in case you need to call some specific function of handshake, decrypt, auth
        //! or something like that.
        SOCKET& UnsafeUnderlyingSocket();

        //! @return Returns the count of data sent.
        [[nodiscard]] StreamSent Send(ByteDataSpan data, const EndpointType& endpoint) const;
        //! @return Returns a generator of each block of data received.
        [[nodiscard]] std::pair<DataStream, EndpointType> Receive() const;
        //! @return Returns all the data blocks until this moment.
        [[nodiscard]] ConnectionResult<ByteData> ReceiveAll() const;

        void Close() const;
        [[nodiscard]] bool IsOpen() const;

    protected:
        SOCKET _socket{ macroINVALID_SOCKET };
    };
} // namespace Hermes

#include <Hermes/Socket/_base/DgramSocket.tpp>
