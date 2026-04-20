/**
 * @file udp_socket.h
 * @brief Cross-platform UDP socket abstraction
 *
 * Provides a simple, non-blocking UDP socket for sending and receiving
 * datagrams. Used as the underlying transport for KCP.
 *
 * Features:
 * - Non-blocking I/O
 * - Reuse address for quick restart
 * - Set receive/send buffer sizes
 * - Windows and POSIX compatibility
 */

#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <string_view>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

namespace PhysicsSync {

// ================================================================
// IP Address / Endpoint
// ================================================================

/**
 * @brief Network endpoint (IP address + port)
 */
struct NetEndpoint {
    std::array<uint8_t, 4> address{};
    uint16_t port = 0;

    NetEndpoint() = default;
    NetEndpoint(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t p)
        : address{a, b, c, d}, port(p) {}

    static NetEndpoint Loopback(uint16_t port) {
        return NetEndpoint(127, 0, 0, 1, port);
    }

    static NetEndpoint Any(uint16_t port) {
        return NetEndpoint(0, 0, 0, 0, port);
    }

    std::string ToString() const {
        return std::to_string(address[0]) + "." +
               std::to_string(address[1]) + "." +
               std::to_string(address[2]) + "." +
               std::to_string(address[3]) + ":" +
               std::to_string(port);
    }
};

// ================================================================
// UDP Socket
// ================================================================

class UDPSocket {
public:
    UDPSocket();
    ~UDPSocket();

    // Non-copyable, non-movable
    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;

    /**
     * @brief Create a UDP socket
     * @return true on success
     */
    bool Create();

    /**
     * @brief Bind to a specific address
     * @param endpoint The endpoint to bind to
     * @param portOnly if true, only bind the port (useful for client)
     * @return true on success
     */
    bool Bind(const NetEndpoint& endpoint, bool portOnly = false);

    /**
     * @brief Set socket to non-blocking mode
     * @param nonBlocking true for non-blocking
     */
    void SetNonBlocking(bool nonBlocking);

    /**
     * @brief Set receive buffer size
     * @param bytes Size in bytes (default 256KB)
     */
    void SetReceiveBufferSize(int bytes);

    /**
     * @brief Set send buffer size
     * @param bytes Size in bytes (default 256KB)
     */
    void SetSendBufferSize(int bytes);

    /**
     * @brief Send data to an endpoint
     * @param data Pointer to data
     * @param size Data size in bytes
     * @param to Destination endpoint
     * @return bytes sent, or -1 on error
     */
    int SendTo(const uint8_t* data, int size, const NetEndpoint& to);

    /**
     * @brief Receive data from any endpoint (with source info)
     * @param buffer Output buffer
     * @param maxSize Maximum receive size
     * @param from Optional pointer to receive source address
     * @return bytes received, or -1 on error
     */
    int ReceiveFrom(uint8_t* buffer, int maxSize, NetEndpoint* from = nullptr);

    /**
     * @brief Check if there is data available to read (non-blocking)
     * @return true if data is available
     */
    bool HasData() const;

    /**
     * @brief Close the socket
     */
    void Close();

    /**
     * @brief Get the native socket handle
     */
#if defined(_WIN32)
    SOCKET NativeHandle() const { return socket_; }
#else
    int NativeHandle() const { return socket_; }
#endif

    explicit operator bool() const { return socket_ != INVALID_SOCKET_VALUE; }

private:
#if defined(_WIN32)
    static constexpr auto INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
    static constexpr int INVALID_SOCKET_VALUE = -1;
#endif

#if defined(_WIN32)
    SOCKET socket_ = INVALID_SOCKET_VALUE;
#else
    int socket_ = INVALID_SOCKET_VALUE;
#endif
};

} // namespace PhysicsSync
