#pragma once
#include <Hermes/Socket/Data/DefaultSocketData.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base.hpp>

#include <array>

namespace Hermes {
    template<SocketTypeEnum SocketType = SocketTypeEnum::Stream>
    struct DefaultTransferPolicy {
        static constexpr auto Type = SocketType;

        static_assert(SocketType != SocketTypeEnum::Dgram, "UDP not supported yet");

        template<ByteLike Byte>
        struct RecvStream : std::ranges::view_interface<RecvStream<Byte>> {
            struct Iterator {
                using difference_type  = std::ptrdiff_t;
                using value_type       = Byte;

                RecvStream* view = nullptr;

                [[nodiscard]] value_type operator*() const;
                Iterator& operator++();
                Iterator& operator++(int);
                [[nodiscard]] bool operator==(std::default_sentinel_t) const;
            };

            template<SocketDataConcept Data>
            explicit RecvStream(Data& data, DefaultTransferPolicy& policy);

            Iterator begin();
            static std::default_sentinel_t end();

            ConnectionResultOper Error() const;
        private:
            ConnectionResultOper Receive();

            SOCKET* _socket;
            DefaultTransferPolicy* _policy;
        };

        template<SocketDataConcept Data, ByteLike Byte>
        StreamByteOper Recv(Data& data, std::span<Byte> bufferRecv, RecvModeEnum recvMode = RecvModeEnum::All);
        template<SocketDataConcept Data, ByteLike Byte>
        static StreamByteOper Send(Data& data, std::span<const Byte> bufferSend);
    private:
        struct State {
            static constexpr size_t bufferSize{ 0x4000 };

            int index{};
            int size{};

            ConnectionResultOper status{};
            std::array<std::byte, bufferSize> buffer{};
        };

        std::unique_ptr<State> _state{ nullptr };

        template<ByteLike Byte>
        static StreamByteOper RecvHelper(SOCKET& socket, std::span<Byte> bufferRecv, RecvModeEnum recvMode);
    };
}

#include <Hermes/Socket/Sync/_base/Transfer/DefaultTransferPolicy.tpp>

namespace Hermes {
    static_assert(std::ranges::viewable_range<DefaultTransferPolicy<>::RecvStream<std::byte>>);
    static_assert(TransferPolicyConcept<DefaultTransferPolicy<>, DefaultSocketData<>>);
}