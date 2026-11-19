#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_ASYNC

#include <Hermes/Socket/Async/_base/ExecutionContext/TransferOperStatus.hpp>

#include <atomic>
#include <concepts>
#include <functional>
#include <optional>
#include <stop_token>

#include <stdexec/execution.hpp>

namespace Hermes::details_ {

    //! @brief Contexts that exposes a cancellation method.
    //! @details Contexts without it keep the previous behaviour, where pending I/O only completes when the backend
    //! completes it. FastIoLoop implements the capability.
    template<class T>
    concept CancelIoCapableContext = requires(const T& ctx, SocketHandle handle, const TransferOperStatus* status) {
        { ctx.CancelIo(handle, status) } -> std::convertible_to<bool>;
    };

    //! @brief Stop-token helper used by async operation policies.
    //! @details Arm() registers a stop callback on the receiver's token. When it fires, the callback sets the flag
    //! and asks the context to cancel the pending IO operation (identified by their `TransferOperStatus`).
    //!
    //! @note Check for `Cancel/Disarm()` before every completion so the callback can never touch an operation state
    //! that already finished.
    //! | Operation                        | Behaviour to implement                                             |
    //! |----------------------------------|--------------------------------------------------------------------|
    //! | IO succeeded                     | Disarm, complete as before (a real completion is never rewritten)  |
    //! | IO failed and Cancelled()        | Disarm, set_stopped                                                |
    //! | IO failed and !Cancelled()       | Disarm, set_error as before                                        |
    //! | stop requested before submitting | Disarm, set_stopped, never submit                                  |
    template<stdexec::stoppable_token StopToken>
    struct StopGuard {
        //! @brief Registers the cancel path for @p token. No-op when the context isn't cancellable (or carries no
        //! context at all).
        template<class Context>
        void Arm(const Context* context, SocketHandle handle, TransferOperStatus* status, StopToken token) noexcept;

        //! @brief Re-issues the cancellation request.
        void CancelNow() const noexcept;

        //! @brief True if requested to stop.
        bool Cancelled() const noexcept;

        //! @brief Unregisters the callback so it can no longer observe this operation state.
        void Disarm() noexcept;

    private:
        using CancelFn = bool (*)(const void*, SocketHandle, const TransferOperStatus*) noexcept;

        template<class Context>
        static bool CancelThunk(const void* context, SocketHandle handle, const TransferOperStatus* status) noexcept;

        std::atomic<bool>               m_stopFired{ false };
        CancelFn                        m_cancelFn{};
        const void*                     m_contextPtr{};
        SocketHandle                    m_handle{};
        TransferOperStatus*             m_status{};
        std::optional<stdexec::stop_callback_for_t<StopToken, std::function<void()>>> m_stopCb{};
    };
}

#include <Hermes/Socket/Async/_base/ExecutionContext/StopGuard.tpp>

#endif
