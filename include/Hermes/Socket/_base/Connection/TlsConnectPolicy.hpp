#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base/Data/TLSSocketData.hpp>
#include <Hermes/Socket/_base/_base.hpp>

namespace Hermes {

    template<SocketDataConcept Data = TlsSocketData<>>
    struct TlsConnectPolicy {
        static ConnectionResultOper Connect(Data& data);
        static ConnectionResultOper Close(Data& data);

    private:
        static ConnectionResultOper ClientHandshake(Data& data);
    };
}

#include <Hermes/Socket/_base/Connection/TlsConnectPolicy.tpp>

namespace Hermes {
        static_assert(ConnectionPolicyConcept<TlsConnectPolicy, TlsSocketData<>>);
}