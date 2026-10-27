#pragma once
#include <Hermes/Endpoint/IpEndpoint/IpEndpoint.hpp>

#include <chrono>
#include <Hermes/Socket/_base.hpp>

namespace Hermes {
    //! @brief A basic data class for general propourse sockets (sync and async).
    //! @details It provides a generic class designed to serve as a data storage to simple socket types.
    //! @tparam ExecutionContext The type of the execution context. `std::monostate` -> no execution context.
    //! @tparam Endpoint The type of the endpoint.
    //! @tparam SocketType The type of the socket.
    //! @tparam SocketFamily The type of the socket family.
    //! @see TlsSocketData for safe TLS data.
    template<
            class             ExecutionContext = std::monostate,
            EndpointConcept   Endpoint         = IpEndpoint,
            SocketTypeEnum    SocketType       = SocketTypeEnum::Stream,
            AddressFamilyEnum SocketFamily     = AddressFamilyEnum::Inet6>
    struct DefaultSocketData {
        using EndpointType = Endpoint;
        static_assert(std::is_class_v<ExecutionContext>, "ExecutionContext must be a class");

        using ExecutionContextType = std::conditional_t<
                std::same_as<ExecutionContext, std::monostate>, std::monostate, ExecutionContext*>;

        static constexpr SocketTypeEnum    Type  { SocketType   };
        static constexpr AddressFamilyEnum Family{ SocketFamily };

        DefaultSocketData() = default;
        explicit DefaultSocketData(Endpoint other);
        ~DefaultSocketData() = default;

        DefaultSocketData(DefaultSocketData&& other) noexcept;
        DefaultSocketData& operator=(DefaultSocketData&& other) noexcept;

        DefaultSocketData MakeChild() const;


        struct SocketOptions {
            // I can't do `using std::chrono_literals::...` thing here, aff
            std::chrono::milliseconds connectTimeout{ std::chrono::seconds{ 30 } };
            std::chrono::milliseconds sendTimeout{ std::chrono::seconds{ 10 } };
            std::chrono::milliseconds recvTimeout{ std::chrono::seconds{ 10 } };
        };

        struct State {
            std::array<std::byte, 0x4000> buffer{};
        };


        Endpoint endpoint{};
        SocketFd socket{ macroINVALID_SOCKET };

        //! @brief The member that stores the buffer of received elements.
        //! @details It's a `std::unique_pointer`, so the class can stay thin and easy to move.
        //! @note The state start's empty, your policies needs to check/fill when using it.
        std::unique_ptr<State> state{};
        //! @brief Timeout options (send/recv timeout, not a deadline).
        SocketOptions options{};
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

    static_assert(SocketDataConcept<DefaultSocketData<>>);
}

#include <Hermes/Socket/Data/DefaultSocketData.tpp>