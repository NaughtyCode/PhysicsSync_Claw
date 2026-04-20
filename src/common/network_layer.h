/**
 * @file network_layer.h
 * @brief KCP-based network layer with packet framing, retransmission, and sequencing
 *
 * This layer wraps KCP and provides:
 * - Message framing with the protocol header
 * - Reliable message delivery via KCP
 * - Message ordering and deduplication
 * - Simple heartbeat / latency tracking
 * - Multi-client support for server mode (per-client KCP instances)
 */

#pragma once

#include "kcp_wrapper.h"
#include "udp_socket.h"
#include "network_protocol.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <deque>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <array>

namespace PhysicsSync {

// ================================================================
// Constants
// ================================================================

constexpr int   NETWORK_MTU              = 1400;
constexpr int   MAX_INFLIGHT_MESSAGES    = 64;
constexpr int   HEARTBEAT_INTERVAL_MS    = 1000;
constexpr float HEARTBEAT_TIMEOUT_S      = 3.0f;
constexpr int   RECONNECT_MAX_DELAY_MS   = 5000;
constexpr size_t FRAME_HEADER_SIZE       = 4; // msgType (2) + payloadLen (2)

// ================================================================
// Forward declarations
// ================================================================

class NetworkLayer;

// Message received callback: (messageBytes, byteCount)
using OnMessageReceived = std::function<void(const uint8_t* data, size_t size)>;
// Disconnected callback
using OnDisconnected = std::function<void()>;

// ================================================================
// Outgoing message
// ================================================================

struct OutgoingMessage {
    uint16_t      type   = 0;
    std::vector<uint8_t> payload;
    uint32_t     seq     = 0;
    uint64_t     sentAt  = 0;   // KCP tick when last sent
    uint8_t      retries = 0;
};

// ================================================================
// Per-client KCP context (server side)
// ================================================================

struct ClientContext {
    std::unique_ptr<KCPWrapper> kcp;
    NetEndpoint endpoint;
    bool connected = false;
};

// ================================================================
// NetworkLayer
// ================================================================

/**
 * @brief Reliable network transport over UDP + KCP
 *
 * Usage (server):
 *   NetworkLayer server;
 *   server.CreateAsServer("0.0.0.0", 9300);
 *   server.Update();
 *   // ...
 *
 * Usage (client):
 *   NetworkLayer client;
 *   client.CreateAsClient("127.0.0.1", 9300);
 *   client.Connect();
 *   client.Update();
 *   // ...
 */
class NetworkLayer : public KCPWrapper::KCPWriter {
public:
    NetworkLayer();
    ~NetworkLayer() override;

    // Non-copyable
    NetworkLayer(const NetworkLayer&) = delete;
    NetworkLayer& operator=(const NetworkLayer&) = delete;

    /**
     * @brief Create socket for server listening on a port
     * @param addr Bind address (e.g. "0.0.0.0")
     * @param port Bind port
     */
    bool CreateAsServer(const char* addr, uint16_t port);

    /**
     * @brief Create socket for client (will connect to server)
     * @param addr Server address
     * @param port Server port
     */
    bool CreateAsClient(const char* addr, uint16_t port);

    /**
     * @brief Perform a non-blocking connect (client only)
     * Returns true when connection is initiated.
     */
    bool Connect();

    /**
     * @brief Must be called periodically (every frame / ~16ms)
     * Processes I/O, drives KCP update, handles heartbeats and retransmits.
     */
    void Update();

    /**
     * @brief Send a constructed NetworkMessage
     * In client mode: sends to the server.
     * In server mode: sends to the given playerId (or to the default peer if no playerId given).
     * @param msg Message to send
     * @param playerId Optional player ID (server only). If 0, sends to default peer.
     * @return true if accepted for sending
     */
    bool Send(std::unique_ptr<NetworkMessage> msg, uint32_t playerId = 0);

    /**
     * @brief Send to a specific peer endpoint directly (server mode)
     * @param msg Message to send
     * @param target The endpoint to send to
     * @return true if accepted
     */
    bool SendToPeer(std::unique_ptr<NetworkMessage> msg, const NetEndpoint& target);

    /**
     * @brief Try to receive a message (non-blocking)
     * @return unique_ptr to message, or nullptr if none available
     */
    std::unique_ptr<NetworkMessage> Receive();

    /**
     * @brief Check if connected to remote
     */
    bool IsConnected() const { return connected_.load(); }

    /**
     * @brief Get last known RTT in milliseconds
     */
    float GetLatency() const { return rttMs_; }

    /**
     * @brief Gracefully close the connection
     */
    void Close();

    /**
     * @brief Set callbacks
     */
    void OnMessage(OnMessageReceived cb)   { onMessage_   = std::move(cb); }
    void OnDisconnectedCb(OnDisconnected cb) { onDisconnected_ = std::move(cb); }

    /**
     * @brief Send raw frame data to a specific player by ID (server only)
     * @param frame The complete frame to send
     * @param playerId The target player ID
     * @return true if sent successfully
     */
    bool SendToPlayer(const std::vector<uint8_t>& frame, uint32_t playerId);

    /**
     * @brief Get connection statistics
     */
    std::string GetStats() const;

public:
    // -- KCPWriter interface implementation --
    int OnOutput(const char* buf, int len) override { return OnKCPOutput(buf, len); }

    // -- Frame / Unframe --
    static bool BuildFrame(uint16_t msgType, const uint8_t* payload,
                           size_t payloadLen, std::vector<uint8_t>& frame);
    static bool Unframe(const uint8_t* data, size_t len,
                        uint16_t& msgType, std::vector<uint8_t>& payload);

    // -- KCP KCPWriter implementation --
    int OnKCPOutput(const char* buf, int len);

    // -- Internal helpers --
    void SendUDP(const uint8_t* data, int size, const NetEndpoint& target);
    void ProcessUDPRcv();
    void ProcessKcpRcv();
    void SendHeartbeat();
    void CheckHeartbeatTimeout();
    void AckReceived(uint32_t seq);
    void FlushRetransmits();
    void HandleIncomingFrame(const uint8_t* data, size_t len, uint32_t playerId = 0);

    // -- Server peer management --
    std::vector<uint32_t> GetAllClientIds() const;

    // -- UDP --
    UDPSocket udp_;
    NetEndpoint serverEndpoint_;  // for client: where to send
    NetEndpoint peerEndpoint_;    // for server: last seen peer (legacy compatibility)

    // -- Client mode KCP --
    std::unique_ptr<KCPWrapper> clientKcp_;

    // -- Server mode: per-client KCP contexts --
    // Maps (ip0<<24|ip1<<16|ip2<<8|ip3) -> ClientContext
    uint32_t GetEndpointKey(const NetEndpoint& ep) const;
    ClientContext* GetOrCreateClientContext(const NetEndpoint& ep);
    ClientContext* GetClientContextByPlayerId(uint32_t playerId) const;
    std::vector<NetEndpoint> GetAllClientEndpoints() const;

    // -- Client endpoint -> playerId mapping --
    std::unordered_map<uint32_t, uint32_t> endpointToPlayerId_; // endpointKey -> playerId
    // -- playerId -> endpoint mapping --
    std::unordered_map<uint32_t, NetEndpoint> playerIdToEndpoint_; // playerId -> endpoint

    // -- KCP --
    std::unique_ptr<KCPWrapper> kcp_;

    // -- Sequencing --
    uint32_t nextSeqOut_  = 0;
    uint32_t lastSeqIn_   = 0;  // highest in-order sequence received
    std::deque<OutgoingMessage> sendQueue_;   // pending / awaiting ACK
    std::unordered_map<uint32_t, uint64_t>   ackedSet_; // sequences acknowledged by peer

    // -- Buffers --
    std::vector<uint8_t> kcpInputBuf_;  // raw UDP data assembled for KCP input
    static constexpr size_t KCP_MAX_FRAME = NETWORK_MTU - 32; // headroom
    std::vector<uint8_t> kcpInputAccum_; // accumulate partial UDP frames

    // -- Heartbeat --
    uint64_t lastHeartbeatTick_  = 0;
    uint64_t lastRcvTimeTick_    = 0;
    float    rttMs_              = 0.0f;

    // -- Server peer management --
    bool isServer_               = false;
    uint32_t nextPlayerId_       = 1;
    uint32_t serverPeerCount_    = 0;

    // -- State --
    std::atomic<bool>   connected_   { false };
    std::atomic<bool>   closing_     { false };

    // -- Receive side --
    std::mutex          recvMutex_;
    std::deque<std::unique_ptr<NetworkMessage>> recvQueue_;

    // -- Statistics --
    uint64_t bytesSent_      = 0;
    uint64_t bytesRcvd_      = 0;
    uint32_t msgsSent_       = 0;
    uint32_t msgsRcvd_       = 0;
    uint32_t retransmits_    = 0;
    uint32_t packetsDropped_ = 0;

    // -- Callbacks --
    OnMessageReceived       onMessage_;
    OnDisconnected          onDisconnected_;
};

} // namespace PhysicsSync
