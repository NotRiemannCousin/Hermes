#pragma once
#include <Hermes/Socket/_base/Accept/TlsAcceptStateMachine.hpp>

namespace Hermes {

    template<SocketDataConcept Data>
    ConnectionResultOper TlsAcceptPolicy<Data>::Listen(Data& data, const int backlog, ListenOptions options) noexcept {
        return DefaultAcceptPolicy<EndpointType, Type, Family>::Listen(data, backlog, options);
    }

    template<SocketDataConcept Data>
    ConnectionResultOper TlsAcceptPolicy<Data>::Accept(Data& data, Data& outData, AcceptOptions options) noexcept {
        outData.acceptStateMachine = std::make_unique<_details::TlsAcceptStateMachine<Data, TlsAcceptPolicy>>(options);

        const auto s_makeHandshake = [&](std::monostate) {
            return TlsAcceptPolicy::ServerHandshake(outData, options);
        };

        return DefaultAcceptPolicy<EndpointType, Type, Family>::Accept(data, outData, options)
                .and_then(s_makeHandshake);
    }

    template<SocketDataConcept Data>
    ConnectionResultOper TlsAcceptPolicy<Data>::ServerHandshake(Data& data, AcceptOptions) noexcept {
        data.acceptStateMachine->SetToOpen();
        if (!data.acceptStateMachine->IsFinished())
            data.acceptStateMachine->Advance(data);

        return data.acceptStateMachine->GetResult();
    }

    template<SocketDataConcept Data>
    void TlsAcceptPolicy<Data>::Close(Data& data) noexcept {
        if (!data.acceptStateMachine) {
            CloseSocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return;
        }

        data.acceptStateMachine->SetToClose();
        data.acceptStateMachine->Advance(data);
    }

    template<SocketDataConcept Data>
    void TlsAcceptPolicy<Data>::Abort(Data& data) noexcept {
        if (!data.acceptStateMachine) {
            CloseSocket(data.socket);
            data.socket = macroINVALID_SOCKET;
            return;
        }

        data.acceptStateMachine->SetToAbort();
        data.acceptStateMachine->Advance(data);
    }
}