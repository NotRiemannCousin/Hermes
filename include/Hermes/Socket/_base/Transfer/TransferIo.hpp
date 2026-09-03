#pragma once
#include <Hermes/Config.hpp>
#if HERMES_ENABLE_TLS

#include <Hermes/Socket/_base.hpp>
#include <chrono>
#include <optional>
#include <algorithm>
#include <limits>

#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
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

        const auto millis{ std::chrono::duration_cast<std::chrono::milliseconds>(remaining) };
        const int timeoutMs{ static_cast<int>(std::min<long long>(millis.count(), std::numeric_limits<int>::max())) };

#ifdef _WIN32
        WSAPOLLFD pfd{};
        pfd.fd = socket;
        pfd.events = readable ? POLLRDNORM : POLLWRNORM;
        return WSAPoll(&pfd, 1, timeoutMs) > 0;
#else
        pollfd pfd{};
        pfd.fd = socket;
        pfd.events = readable ? POLLIN : POLLOUT;
        return poll(&pfd, 1, timeoutMs) > 0;
#endif
    }
}

#endif