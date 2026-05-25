#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/Data/TlsSocketData.hpp>
#include <Hermes/Socket/_base.hpp>
#include <memory>
#include <span>

namespace Hermes {
    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsTransferPolicy {
        static constexpr auto Type{ Data::Type };

        template<ByteLike Byte = std::byte>
        struct RecvStream : std::ranges::view_interface<RecvStream<Byte>> {
            struct Iterator {
                using difference_type = std::ptrdiff_t;
                using value_type      = Byte;

                RecvStream* view{ nullptr };

                [[nodiscard]] value_type operator*() const;
                Iterator& operator++();
                Iterator& operator++(int);
                bool operator==(std::default_sentinel_t) const;
            };

            explicit RecvStream(Data& data, TlsTransferPolicy& policy);

            Iterator begin();
            static std::default_sentinel_t end();

            ConnectionResultOper Error() const;
        private:
            ConnectionResultOper Receive();

            Data* _data;
            TlsTransferPolicy* _policy;
        };

        StreamByteOper Recv(Data& data, std::span<std::byte> bufferRecv, RecvModeEnum recvMode = RecvModeEnum::All) noexcept;

        StreamByteOper Send(Data& data, std::span<const std::byte> bufferSend) noexcept;

    private:
        struct StreamState {
            size_t index{};
            size_t size{};
            std::array<std::byte, 4096> buffer{};
            ConnectionResultOper status{};
        };

        std::unique_ptr<StreamState> _streamState{ nullptr };

        // template<ByteLike Byte> friend struct RecvStream;
    };
}

#include <Hermes/Socket/Sync/_base/Transfer/TlsTransferPolicy.tpp>

namespace Hermes {
    static_assert(std::ranges::viewable_range<TlsTransferPolicy<>::RecvStream<>>);
    static_assert(TransferPolicyConcept<TlsTransferPolicy<>, TlsSocketData<>>);
}