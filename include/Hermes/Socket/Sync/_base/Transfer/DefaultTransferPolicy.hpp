#pragma once
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base.hpp>
#include <Hermes/Socket/_base/Transfer/TransferIo.hpp>

#include <array>

namespace Hermes {
    template<SocketTypeEnum SocketType = SocketTypeEnum::Stream>
    struct DefaultTransferPolicy {
        using Deadline = details_::TransferDeadline;
        //! @brief Options for a bounded receive operation.
        //! @details `deadline` is absolute and is shared by every partial receive in the call.
        struct RecvOptions {
            std::optional<Deadline> deadline{};
        };
        //! @brief Options for a bounded send operation.
        //! @details `deadline` is absolute and is shared by every partial send in the call.
        struct SendOptions {
            std::optional<Deadline> deadline{};
        };
        static constexpr auto Type{ SocketType };

        static_assert(SocketType != SocketTypeEnum::Dgram, "UDP not supported yet");

        template<ByteLike Byte = std::byte>
        struct RecvStream : std::ranges::view_interface<RecvStream<Byte>> {
            struct Iterator {
                using difference_type  = std::ptrdiff_t;
                using value_type       = Byte;

                RecvStream* view = nullptr;

                [[nodiscard]] Byte operator*() const;
                Iterator& operator++();
                Iterator& operator++(int);
                [[nodiscard]] bool operator==(std::default_sentinel_t) const;
            };

            template<SocketDataConcept Data>
            explicit RecvStream(Data& data, DefaultTransferPolicy& policy)
                requires std::default_initializable<RecvOptions>;

            template<SocketDataConcept Data>
            explicit RecvStream(Data& data, DefaultTransferPolicy& policy, RecvOptions options);

            Iterator begin();
            static std::default_sentinel_t end();

            ConnectionResultOper Error() const;
        private:
            ConnectionResultOper Receive();

            SocketFd* m_socket;
            DefaultTransferPolicy* m_policy;
            RecvOptions m_options{};

        };

        template<SocketDataConcept Data>
        StreamByteOper Recv(
            Data& data,
            std::span<std::byte> bufferRecv,
            RecvModeEnum recvMode = RecvModeEnum::All,
            RecvOptions options = {}
        );
        template<SocketDataConcept Data>
        static StreamByteOper Send(
            Data& data,
            std::span<const std::byte> bufferSend,
            SendOptions options = {}
        );
    private:
        struct State {
            static constexpr size_t bufferSize{ 0x4000 };

            int index{};
            int size{};

            ConnectionResultOper status{};
            std::array<std::byte, bufferSize> buffer{};
        };

        std::unique_ptr<State> m_state{ nullptr };

        static StreamByteOper RecvHelper(
            SocketFd& socket,
            std::span<std::byte> bufferRecv,
            RecvModeEnum recvMode,
            const RecvOptions& options
        );
    };
}

#include <Hermes/Socket/Sync/_base/Transfer/DefaultTransferPolicy.tpp>

namespace Hermes {
    static_assert(std::ranges::viewable_range<DefaultTransferPolicy<>::RecvStream<>>);
    static_assert(TransferPolicyConcept<DefaultTransferPolicy<>, DefaultSocketData<>>);
}