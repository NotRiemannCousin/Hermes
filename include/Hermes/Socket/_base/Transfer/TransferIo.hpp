#pragma once
#include <Hermes/Socket/_base.hpp>
#include <chrono>
#include <optional>

#ifndef _WIN32
#include <fcntl.h>
#endif

namespace Hermes::details_ {
    using TransferDeadline = std::chrono::steady_clock::time_point;

    class ScopedNonBlocking {
    public:
        ScopedNonBlocking(const SocketFd socket, const bool enabled) noexcept
            : m_socket{ socket } {
            if (!enabled) return;

#ifdef _WIN32
            u_long mode{ 1 };
            m_active = ioctlsocket(m_socket, FIONBIO, &mode) == 0;
#else
            m_flags = fcntl(m_socket, F_GETFL, 0);
            if (m_flags < 0) return;
            m_active = fcntl(m_socket, F_SETFL, m_flags | O_NONBLOCK) == 0;
#endif
        }

        ~ScopedNonBlocking() {
            if (!m_active) return;

#ifdef _WIN32
            u_long mode{ 0 };
            ioctlsocket(m_socket, FIONBIO, &mode);
#else
            fcntl(m_socket, F_SETFL, m_flags);
#endif
        }

        ScopedNonBlocking(const ScopedNonBlocking&) = delete;
        ScopedNonBlocking& operator=(const ScopedNonBlocking&) = delete;

    private:
        SocketFd m_socket;
        bool m_active{ false };
#ifndef _WIN32
        int m_flags{ 0 };
#endif
    };

    inline bool WaitForSocket(
        const SocketFd socket,
        const bool readable,
        const std::optional<TransferDeadline>& deadline
    ) noexcept {
        if (!deadline.has_value())
            return true;
        const auto remaining{ *deadline - std::chrono::steady_clock::now() };
        if (remaining <= std::chrono::steady_clock::duration::zero())
            return false;
        fd_set readSet, writeSet;
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        FD_SET(socket, readable ? &readSet : &writeSet);
        const auto micros{ std::chrono::duration_cast<std::chrono::microseconds>(remaining) };
        timeval timeout{
            static_cast<long>(micros.count() / 1'000'000),
            static_cast<long>(micros.count() % 1'000'000)
        };
        return select(
            static_cast<int>(socket) + 1,
            readable ? &readSet : nullptr,
            readable ? nullptr : &writeSet,
            nullptr,
            &timeout
        ) > 0;
    }
}
