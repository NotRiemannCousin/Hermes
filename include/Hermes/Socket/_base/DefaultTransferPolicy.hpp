#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base/_base.hpp>
#include <Hermes/Socket/_base/DefaultSocketData.hpp>
#include <Hermes/_base/WinAPI.hpp>

#include <chrono>
#include <array>


namespace Hermes {
    template<SocketDataConcept Data = DefaultSocketData<>>
    struct DefaultTransferPolicy {
        struct RecvRange {
            struct Iterator {
                using difference_type  = std::ptrdiff_t;
                using value_type       = std::byte;

                RecvRange* view = nullptr;

                [[nodiscard]] std::byte operator*() const;
                Iterator& operator++();
                Iterator& operator++(int);
                [[nodiscard]] bool operator==(std::default_sentinel_t) const;
            };

            explicit RecvRange(DefaultTransferPolicy& policy);

            Iterator begin();
            static std::default_sentinel_t end();

            ConnectionResultOper OptError() const;
        private:
            ConnectionResultOper Receive();

            static constexpr size_t bufferSize{ 0x0F00 };

            int _index{};
            int _size{};
            DefaultTransferPolicy& _policy;
            ConnectionResultOper _errorStatus{};
            std::array<std::byte, 0x0F00> _buffer{};
        };


        ConnectionResultOper Recv(Data& data, std::span<std::byte> bufferRecv);
        ConnectionResultOper Send(Data& data, std::span<const std::byte> bufferSend);

    private:
        SOCKET _socket{};
    };
}

#include <Hermes/Socket/_base/DefaultTransferPolicy.tpp>

namespace Hermes {
    static_assert(std::ranges::range<DefaultTransferPolicy<>::RecvRange>);
    static_assert(TransferPolicyConcept<DefaultTransferPolicy, DefaultSocketData<>>);
}