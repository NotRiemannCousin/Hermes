#pragma once
#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <Hermes/Socket/_base/Data/DefaultSocketData.hpp>
#include <Hermes/Socket/_base/_base.hpp>

namespace Hermes {

    template<SocketDataConcept Data = DefaultSocketData<>>
    struct DefaultConnectPolicy {
        static ConnectionResultOper Connect(Data& data);
        static ConnectionResultOper Close(Data& data);
    };
}

#include <Hermes/Socket/_base/Connection/DefaultConnectPolicy.tpp>

namespace Hermes {
        static_assert(ConnectionPolicyConcept<DefaultConnectPolicy, DefaultSocketData<>>);
}