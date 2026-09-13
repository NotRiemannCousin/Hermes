#include <Hermes/Socket/Sync/ClientSocket.hpp>
#include <Hermes/Socket/Sync/ListenerSocket.hpp>
#include <Hermes/Socket/Sync/ServerSocket.hpp>

#if HERMES_ENABLE_ASYNC
#include <Hermes/Socket/Async/AsyncClientSocket.hpp>
#include <Hermes/Socket/Async/AsyncListenerSocket.hpp>
#include <Hermes/Socket/Async/AsyncServerSocket.hpp>
#endif



namespace Hermes {
    template struct ClientSocket<DefaultSocketData<>, DefaultConnectPolicy<>, DefaultTransferPolicy<>>;
    template struct ListenerSocket<DefaultSocketData<>, DefaultAcceptPolicy<>, DefaultTransferPolicy<>>;
    template struct ServerSocket<DefaultSocketData<>, DefaultAcceptPolicy<>, DefaultTransferPolicy<>>;
#if HERMES_ENABLE_TLS
    template struct ClientSocket<TlsSocketData<>, TlsConnectPolicy<>, TlsTransferPolicy<>>;
    template struct ListenerSocket<TlsSocketData<>, TlsAcceptPolicy<>, TlsTransferPolicy<>>;
    template struct ServerSocket<TlsSocketData<>, TlsAcceptPolicy<>, TlsTransferPolicy<>>;
#endif

#if HERMES_ENABLE_ASYNC
    template struct AsyncClientSocket<DefaultSocketData<>, DefaultAsyncConnectPolicy<>, DefaultAsyncTransferPolicy<>>;
    template struct AsyncListenerSocket<DefaultSocketData<>, DefaultAsyncAcceptPolicy<>, DefaultAsyncTransferPolicy<>>;
    template struct AsyncServerSocket<DefaultSocketData<>, DefaultAsyncAcceptPolicy<>, DefaultAsyncTransferPolicy<>>;

#if HERMES_ENABLE_TLS && HERMES_ENABLE_NATIVE_SCHEDULER
    template struct AsyncClientSocket<TlsSocketData<>, TlsAsyncConnectPolicy<>, TlsAsyncTransferPolicy<>>;
    template struct AsyncListenerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy<>, TlsAsyncTransferPolicy<>>;
    template struct AsyncServerSocket<TlsSocketData<>, TlsAsyncAcceptPolicy<>, TlsAsyncTransferPolicy<>>;
#endif
#endif
}



