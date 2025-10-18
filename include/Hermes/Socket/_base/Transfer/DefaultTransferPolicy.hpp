#pragma once
#include <Hermes/Socket/_base/Data/DefaultSocketData.hpp>
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base/_base.hpp>

#include <chrono>
#include <array>


namespace Hermes {
    template<SocketDataConcept Data = DefaultSocketData<>>
    struct DefaultTransferPolicy {
        template<ByteLike Byte>
        struct RecvRange {
            struct Iterator {
                using difference_type  = std::ptrdiff_t;
                using value_type       = Byte;

                RecvRange* view = nullptr;

                [[nodiscard]] value_type operator*() const;
                Iterator& operator++();
                Iterator& operator++(int);
                [[nodiscard]] bool operator==(std::default_sentinel_t) const;
            };

            explicit RecvRange(Data& data, DefaultTransferPolicy& policy);

            Iterator begin();
            static std::default_sentinel_t end();

            ConnectionResultOper Error() const;
        private:
            ConnectionResultOper Receive();

            static constexpr size_t bufferSize{ 0x4000 };

            int _index{};
            int _size{};
            Data& _data;
            ConnectionResultOper _errorStatus{};
            std::array<std::byte, bufferSize> _buffer{};
        };

        template<ByteLike Byte>
        static StreamByteOper Recv(Data& data, std::span<Byte> bufferRecv);
        template<ByteLike Byte>
        static StreamByteOper Send(Data& data, std::span<const Byte> bufferSend);
    private:
        template<ByteLike Byte>
        static StreamByteOper RecvHelper(Data& data, std::span<Byte> bufferRecv, bool single);
    };
}

#include <Hermes/Socket/_base/Transfer/DefaultTransferPolicy.tpp>

namespace Hermes {
    static_assert(std::ranges::range<DefaultTransferPolicy<>::RecvRange<std::byte>>);
    static_assert(TransferPolicyConcept<DefaultTransferPolicy, DefaultSocketData<>>);
}