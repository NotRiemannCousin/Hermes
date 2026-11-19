#pragma once

#if HERMES_ENABLE_ASYNC

namespace Hermes::details_ {
    template<stdexec::stoppable_token StopToken>
    template<class Context>
    void StopGuard<StopToken>::Arm(const Context* context, SocketHandle handle, TransferOperStatus* status, StopToken token) noexcept {
        if constexpr (CancelIoCapableContext<Context>) {
            if (!token.stop_possible() || context == nullptr || status == nullptr)
                return;

            m_cancelFn   = &CancelThunk<Context>;
            m_contextPtr = context;
            m_handle     = handle;
            m_status     = status;

            m_stopCb.emplace(std::move(token), [this]() noexcept {
                m_stopFired.store(true, std::memory_order_seq_cst);
                CancelNow();
            });
        }
    }

    template<stdexec::stoppable_token StopToken>
    void StopGuard<StopToken>::CancelNow() const noexcept {
        if (m_cancelFn && m_contextPtr)
            static_cast<void>(m_cancelFn(m_contextPtr, m_handle, m_status));
    }

    template<stdexec::stoppable_token StopToken>
    bool StopGuard<StopToken>::Cancelled() const noexcept {
        return m_stopFired.load(std::memory_order_seq_cst);
    }

    template<stdexec::stoppable_token StopToken>
    void StopGuard<StopToken>::Disarm() noexcept {
        m_stopCb.reset();
    }

    template<stdexec::stoppable_token StopToken>
    template<class Context>
    bool StopGuard<StopToken>::CancelThunk(const void* context, SocketHandle handle, const TransferOperStatus* status) noexcept {
        return static_cast<const Context*>(context)->CancelIo(handle, status);
    }
}

#endif
