#pragma once
#if HERMES_ENABLE_TLS

#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>
#include <Hermes/Socket/_base/TlsSession.hpp>
#include <Hermes/Socket/_base.hpp>
#include <Hermes/_base/OsApi/OsApi.hpp>
#include <Hermes/_base/Credentials.hpp>
#include <memory>

namespace Hermes::details_ {
    template<typename Data>
    struct ITlsConnectStateMachine;
    template<typename Data>
    struct ITlsTransferStateMachine;
    template<typename Data>
    struct ITlsAcceptStateMachine;
}

namespace Hermes {

    //! @brief A safe TLS data class for general propourse sockets (sync and async).
    //! @details It provides a generic class designed to serve as a data storage to TLS safe socket tupes.
    //! @tparam ExecutionContext The type of the execution context. `std::monostate` -> no execution context.
    //! @tparam Endpoint The type of the endpoint.
    //! @tparam SocketType The type of the socket.
    //! @tparam SocketFamily The type of the socket family.
    //! @see TlsSocketData.
    template<
            class             ExecutionContext = std::monostate,
            EndpointConcept   Endpoint         = IpEndpoint,
            SocketTypeEnum    SocketType       = SocketTypeEnum::Stream,
            AddressFamilyEnum SocketFamily     = AddressFamilyEnum::Inet6>
    struct TlsSocketData {
        using EndpointType = Endpoint;

        static_assert(SocketType == SocketTypeEnum::Stream && "DTLS not implemented yet");
        static_assert(std::is_class_v<ExecutionContext>, "ExecutionContext must be a class");

        using ExecutionContextType = std::conditional_t<
                std::same_as<ExecutionContext, std::monostate>, std::monostate, ExecutionContext*>;

        static constexpr SocketTypeEnum    Type  { SocketType   };
        static constexpr AddressFamilyEnum Family{ SocketFamily };

        TlsSocketData() = default;
        TlsSocketData(Endpoint endpoint, std::string host, const Credentials* credentials = nullptr);
        ~TlsSocketData();

        TlsSocketData(TlsSocketData&& other) noexcept;
        TlsSocketData& operator=(TlsSocketData&& other) noexcept;

        TlsSocketData MakeChild() const;

        //! @brief The host name of the remote endpoint.
        std::string host{};

        //! @brief A const reference to the current credentials. Can be from client or server.
        const Credentials* credentials{};

        std::unique_ptr<details_::ITlsConnectStateMachine<TlsSocketData>>  connectStateMachine;  //!< TLS state machine.
        std::unique_ptr<details_::ITlsTransferStateMachine<TlsSocketData>> transferStateMachine; //!< TLS state machine.
        std::unique_ptr<details_::ITlsAcceptStateMachine<TlsSocketData>>   acceptStateMachine;   //!< TLS state machine.

        //! @brief In a TLS renegotiation, this field will store the count of bytes that are alreaded decripted.
        uint32_t pendingData{};

        Endpoint endpoint{};
        SocketFd socket{ macroINVALID_SOCKET };

        struct State {
            std::array<std::byte, 0x4800>  encryptedData{};
            std::array<std::byte, 0x10000> decryptedData{};

            std::span<std::byte> decryptedDataSpan{};
            std::span<std::byte> decryptedExtraSpan{};
        };

        //! @brief The member that stores the buffers that stores the decripted/encripted data.
        //! @details It's a `std::unique_pointer`, so the class can stay thin and easy to move.
        //! @note The state start's empty, your policies needs to check/fill when using it.
        std::unique_ptr<State> state{};

        //! @brief The TLS session info.
        details_::TlsSession session{};


        //! @brief The execution context for async sockets.
        //! @details In IO operations, ideally your execution context state is stored somewhere else with a longer
        //! lifetime than the sockets that uses it, so this data class stores a non-owning reference of this execution
        //! context.
        //! @note This is a pointer unless ExecutionContext is a `std::monostate`. In that case, this member is a
        //! `std::monostate` with no unique address.
        //! @warning It's a raw pointer so the value can be `nullptr`. This is the default value, your classes needs to
        //! deal with that.
        [[no_unique_address]] ExecutionContextType executionContext{};

    };

    static_assert(SocketDataConcept<TlsSocketData<>>);
}

#include <Hermes/Socket/Data/TlsSocketData.tpp>

#endif