#pragma once
#include <Hermes/Socket/_base/Connection/TlsConnectStateMachine.hpp>

namespace Hermes {

    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::Connect(Data& data, Options options) noexcept {
        data.connectStateMachine = std::make_unique<_details::TlsConnectStateMachine<Data, TlsConnectPolicy>>(options);

        const auto s_makeHandshake = [this, &data](std::monostate) {
            return this->ClientHandshake(data);
        };

        return DefaultConnectPolicy<typename Data::EndpointType, SocketTypeEnum::Stream, Data::Family>::Connect(data, options)
                .and_then(s_makeHandshake);
    }

    // I hate TLS so much

    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::ClientHandshake(Data& data) {
        data.connectStateMachine->SetToOpen();

        if (!data.connectStateMachine->IsFinished())
            data.connectStateMachine->Advance(data);

        return data.connectStateMachine->GetResult();
    }

    template<SocketDataConcept Data>
    void TlsConnectPolicy<Data>::Close(Data& data) {
        data.connectStateMachine->SetToClose();

        data.connectStateMachine->Advance(data);
    }

    template<SocketDataConcept Data>
    void TlsConnectPolicy<Data>::Abort(Data& data) {
        data.connectStateMachine->SetToAbort();

        data.connectStateMachine->Advance(data);
    }

    template<SocketDataConcept Data>
    ConnectionResultOper TlsConnectPolicy<Data>::Renegotiate(Data& data) {
        data.connectStateMachine->SetToOpen();

        if (!data.connectStateMachine->IsFinished())
            data.connectStateMachine->Advance(data);

        return data.connectStateMachine->GetResult();
    }
}