/**
 * @file udp_socket.cpp
 * @brief Cross-platform UDP socket implementation
 */

#include "udp_socket.h"

#if defined(_WIN32)
    #include <WS2tcpip.h>
#else
    #include <fcntl.h>
    #include <errno.h>
#endif

namespace PhysicsSync {

UDPSocket::UDPSocket() : socket_(INVALID_SOCKET_VALUE) {
#if defined(_WIN32)
    // Initialize Winsock once
    static bool initialized = []() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        return true;
    }();
    (void)initialized;
#endif
}

UDPSocket::~UDPSocket() {
    Close();
}

bool UDPSocket::Create() {
    if (socket_ != INVALID_SOCKET_VALUE) {
        Close();
    }

#if defined(_WIN32)
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
        return false;
    }
#else
    socket_ = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
    if (socket_ < 0) {
        return false;
    }
    // If SOCK_NONBLOCK not available, use fcntl
    if (socket_ >= 0) {
        int flags = fcntl(socket_, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
        }
    }
#endif

    // Allow address reuse
    int optval = 1;
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&optval), sizeof(optval));

    // Set large receive buffer
    SetReceiveBufferSize(256 * 1024);
    SetSendBufferSize(256 * 1024);

    return true;
}

bool UDPSocket::Bind(const NetEndpoint& endpoint, bool portOnly) {
    if (!socket_) {
        Create();
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint.port);

    if (portOnly) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        uint32_t ip = (static_cast<uint32_t>(endpoint.address[0]) << 24) |
                      (static_cast<uint32_t>(endpoint.address[1]) << 16) |
                      (static_cast<uint32_t>(endpoint.address[2]) << 8) |
                      static_cast<uint32_t>(endpoint.address[3]);
        addr.sin_addr.s_addr = htonl(ip);
    }

#if defined(_WIN32)
    return bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
#else
    return bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
#endif
}

void UDPSocket::SetNonBlocking(bool nonBlocking) {
#if defined(_WIN32)
    u_long mode = nonBlocking ? 1 : 0;
    ioctlsocket(socket_, FIONBIO, &mode);
#else
    int flags = fcntl(socket_, F_GETFL, 0);
    if (flags >= 0) {
        if (nonBlocking) {
            fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
        } else {
            fcntl(socket_, F_SETFL, flags & ~O_NONBLOCK);
        }
    }
#endif
}

void UDPSocket::SetReceiveBufferSize(int bytes) {
    if (!socket_) return;
    setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
               reinterpret_cast<const char*>(&bytes), sizeof(bytes));
}

void UDPSocket::SetSendBufferSize(int bytes) {
    if (!socket_) return;
    setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
               reinterpret_cast<const char*>(&bytes), sizeof(bytes));
}

int UDPSocket::SendTo(const uint8_t* data, int size, const NetEndpoint& to) {
    if (!socket_ || !data || size <= 0) {
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(to.port);

    uint32_t ip = (static_cast<uint32_t>(to.address[0]) << 24) |
                  (static_cast<uint32_t>(to.address[1]) << 16) |
                  (static_cast<uint32_t>(to.address[2]) << 8) |
                  static_cast<uint32_t>(to.address[3]);
    addr.sin_addr.s_addr = htonl(ip);

#if defined(_WIN32)
    int sent = sendto(socket_,
                      reinterpret_cast<const char*>(data), size, 0,
                      reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        // WSAEWOULDBLOCK means non-blocking and would block - treat as transient
        if (err == WSAEWOULDBLOCK || err == WSAECONNRESET) {
            return -1;
        }
        return -1;
    }
    return sent;
#else
    ssize_t sent = sendto(socket_, data, size, 0,
                          reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (sent < 0) {
        int err = errno;
        if (err == EAGAIN || err == EWOULDBLOCK || err == ECONNRESET) {
            return -1;
        }
        return -1;
    }
    return static_cast<int>(sent);
#endif
}

int UDPSocket::ReceiveFrom(uint8_t* buffer, int maxSize, NetEndpoint* from) {
    if (!socket_ || !buffer || maxSize <= 0) {
        return -1;
    }

    sockaddr_in addr{};
    socklen_t addrLen = sizeof(addr);

#if defined(_WIN32)
    int received = recvfrom(socket_,
                            reinterpret_cast<char*>(buffer), maxSize, 0,
                            reinterpret_cast<sockaddr*>(&addr), &addrLen);
    if (received == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAECONNRESET) {
            return 0;  // No data available (non-blocking)
        }
        return -1;
    }
#else
    ssize_t received = recvfrom(socket_, buffer, maxSize, 0,
                                reinterpret_cast<sockaddr*>(&addr), &addrLen);
    if (received < 0) {
        int err = errno;
        if (err == EAGAIN || err == EWOULDBLOCK) {
            return 0;  // No data available (non-blocking)
        }
        return -1;
    }
#endif

    if (from) {
        uint32_t ip = ntohl(addr.sin_addr.s_addr);
        (*from).address[0] = (ip >> 24) & 0xFF;
        (*from).address[1] = (ip >> 16) & 0xFF;
        (*from).address[2] = (ip >> 8) & 0xFF;
        (*from).address[3] = ip & 0xFF;
        (*from).port = ntohs(addr.sin_port);
    }

    return static_cast<int>(received);
}

bool UDPSocket::HasData() const {
    if (!socket_) return false;

#if defined(_WIN32)
    u_long bytesAvailable = 0;
    if (ioctlsocket(socket_, FIONREAD, &bytesAvailable) != SOCKET_ERROR) {
        return bytesAvailable > 0;
    }
    return false;
#else
    u_long bytesAvailable = 0;
    if (ioctl(socket_, FIONREAD, &bytesAvailable) == 0) {
        return bytesAvailable > 0;
    }
    return false;
#endif
}

void UDPSocket::Close() {
    if (socket_ == INVALID_SOCKET_VALUE) {
        return;
    }

#if defined(_WIN32)
    closesocket(socket_);
#else
    close(socket_);
#endif
    socket_ = INVALID_SOCKET_VALUE;
}

} // namespace PhysicsSync
