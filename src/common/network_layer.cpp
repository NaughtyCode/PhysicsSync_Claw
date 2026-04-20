/**
 * @file network_layer.cpp
 * @brief Reliable network transport over raw UDP
 *
 * Implements message framing and a clean API for server and client.
 * Uses raw UDP - no KCP.
 */

#include "network_layer.h"
#include <sstream>
#include <cstring>
#include <chrono>

namespace PhysicsSync {

static uint64_t NowTickMS() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()
    );
}

static bool ParseEndpoint(const char* addrStr, uint16_t port, NetEndpoint* out) {
    if (!out) return false;
    unsigned a = 0, b = 0, c = 0, d = 0;
    char dummy;
    if (sscanf(addrStr, "%u.%u.%u.%u%c", &a, &b, &c, &d, &dummy) == 4) {
        if (a > 255 || b > 255 || c > 255 || d > 255) return false;
        out->address[0] = static_cast<uint8_t>(a);
        out->address[1] = static_cast<uint8_t>(b);
        out->address[2] = static_cast<uint8_t>(c);
        out->address[3] = static_cast<uint8_t>(d);
        out->port = port;
        return true;
    }
    if (strcmp(addrStr, "0.0.0.0") == 0 || strcmp(addrStr, "") == 0) {
        *out = NetEndpoint::Any(port);
        return true;
    }
    return false;
}

// ================================================================
// NetworkLayer
// ================================================================

NetworkLayer::NetworkLayer()
    : kcpInputBuf_(NETWORK_MTU)
    , kcpInputAccum_(NETWORK_MTU * 4)
{
}

NetworkLayer::~NetworkLayer() {
    Close();
}

bool NetworkLayer::BuildFrame(uint16_t msgType, const uint8_t* payload,
                               size_t payloadLen, std::vector<uint8_t>& frame) {
    frame.clear();
    frame.reserve(FRAME_HEADER_SIZE + payloadLen);
    frame.push_back(static_cast<uint8_t>(msgType & 0xFF));
    frame.push_back(static_cast<uint8_t>((msgType >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(payloadLen & 0xFF));
    frame.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xFF));
    if (payload && payloadLen > 0) {
        frame.insert(frame.end(), payload, payload + payloadLen);
    }
    return true;
}

bool NetworkLayer::Unframe(const uint8_t* data, size_t len,
                            uint16_t& msgType, std::vector<uint8_t>& payload) {
    if (len < FRAME_HEADER_SIZE) return false;
    msgType = static_cast<uint16_t>(data[0] | (data[1] << 8));
    uint16_t payloadLen = static_cast<uint16_t>(data[2] | (data[3] << 8));
    if (len < FRAME_HEADER_SIZE + payloadLen) return false;
    payload.assign(data + FRAME_HEADER_SIZE, data + FRAME_HEADER_SIZE + payloadLen);
    return true;
}

int NetworkLayer::OnKCPOutput(const char* buf, int len) {
    (void)buf; (void)len;
    return 0;
}

bool NetworkLayer::CreateAsServer(const char* addr, uint16_t port) {
    isServer_ = true;
    if (!udp_.Create()) return false;
    NetEndpoint bindAddr = NetEndpoint::Any(port);
    if (addr && strcmp(addr, "0.0.0.0") != 0) {
        ParseEndpoint(addr, port, &bindAddr);
    }
    if (!udp_.Bind(bindAddr)) return false;
    udp_.SetNonBlocking(true);
    connected_.store(true);
    serverPeerCount_ = 0;
    nextPlayerId_ = 1;
    return true;
}

bool NetworkLayer::CreateAsClient(const char* addr, uint16_t port) {
    isServer_ = false;
    if (!udp_.Create()) return false;
    udp_.SetNonBlocking(true);
    if (!ParseEndpoint(addr, port, &serverEndpoint_)) return false;
    return true;
}

bool NetworkLayer::Connect() {
    if (isServer_) return false;
    auto msg = std::make_unique<ConnectRequestMessage>();
    Send(std::move(msg));
    return true;
}

// -- Main update loop --

void NetworkLayer::Update() {
    if (!udp_ || closing_.load()) return;

    uint64_t now = NowTickMS();

    // 1. Receive UDP datagrams and parse frames directly
    ProcessUDPRcv();

    // 2. Heartbeat
    if (now - lastHeartbeatTick_ >= HEARTBEAT_INTERVAL_MS) {
        SendHeartbeat();
    }
    CheckHeartbeatTimeout();
}

// -- Send --

bool NetworkLayer::Send(std::unique_ptr<NetworkMessage> msg, uint32_t playerId) {
    if (!msg) return false;

    std::vector<uint8_t> payload;
    msg->Serialize(payload);

    std::vector<uint8_t> frame;
    BuildFrame(msg->GetType(), payload.data(), payload.size(), frame);

    if (isServer_) {
        // Server mode: find target endpoint by playerId and send directly via UDP
        auto it = playerIdToEndpoint_.find(playerId);
        if (it != playerIdToEndpoint_.end()) {
            int rc = udp_.SendTo(frame.data(), static_cast<int>(frame.size()), it->second);
            if (rc > 0) bytesSent_ += static_cast<uint64_t>(rc);
            msgsSent_++;
            return rc > 0;
        }
        // Fallback: send to last known peer
        if (playerId == 0 && !peerEndpoint_.port) return false;
        if (playerId == 0) {
            int rc = udp_.SendTo(frame.data(), static_cast<int>(frame.size()), peerEndpoint_);
            if (rc > 0) bytesSent_ += static_cast<uint64_t>(rc);
            msgsSent_++;
            return rc > 0;
        }
        return false;
    }

    // Client mode: send directly to server
    int rc = udp_.SendTo(frame.data(), static_cast<int>(frame.size()), serverEndpoint_);
    if (rc > 0) bytesSent_ += static_cast<uint64_t>(rc);
    msgsSent_++;
    return rc > 0;
}

bool NetworkLayer::SendToPeer(std::unique_ptr<NetworkMessage> msg, const NetEndpoint& target) {
    if (!msg) return false;
    std::vector<uint8_t> payload;
    msg->Serialize(payload);
    std::vector<uint8_t> frame;
    BuildFrame(msg->GetType(), payload.data(), payload.size(), frame);
    int rc = udp_.SendTo(frame.data(), static_cast<int>(frame.size()), target);
    if (rc > 0) bytesSent_ += static_cast<uint64_t>(rc);
    msgsSent_++;
    return rc > 0;
}

// -- Receive --

std::unique_ptr<NetworkMessage> NetworkLayer::Receive() {
    std::lock_guard<std::mutex> lock(recvMutex_);
    if (recvQueue_.empty()) return nullptr;
    auto msg = std::move(recvQueue_.front());
    recvQueue_.pop_front();
    return msg;
}

// -- Close --

void NetworkLayer::Close() {
    closing_.store(true);
    connected_.store(false);
    if (clientKcp_) clientKcp_.reset();
    if (kcp_) kcp_.reset();
    udp_.Close();
    sendQueue_.clear();
    {
        std::lock_guard<std::mutex> lock(recvMutex_);
        recvQueue_.clear();
    }
    ackedSet_.clear();
    playerIdToEndpoint_.clear();
    endpointToPlayerId_.clear();
    closing_.store(false);
}

// -- Statistics --

std::string NetworkLayer::GetStats() const {
    std::ostringstream oss;
    oss << "NetworkLayer{"
        << "connected=" << connected_.load()
        << ", isServer=" << isServer_
        << ", rtt=" << rttMs_ << "ms"
        << ", sent=" << msgsSent_
        << ", rcvd=" << msgsRcvd_
        << ", bytes_sent=" << bytesSent_
        << ", bytes_rcvd=" << bytesRcvd_
        << ", peers=" << serverPeerCount_
        << "}";
    return oss.str();
}

// -- Internal helpers --

void NetworkLayer::SendUDP(const uint8_t* data, int size, const NetEndpoint& target) {
    if (!udp_ || size <= 0) return;
    int sent = udp_.SendTo(data, size, target);
    if (sent > 0) bytesSent_ += static_cast<uint64_t>(sent);
}

void NetworkLayer::ProcessUDPRcv() {
    while (udp_.HasData()) {
        int n = udp_.ReceiveFrom(kcpInputBuf_.data(),
                                  static_cast<int>(kcpInputBuf_.size()),
                                  &peerEndpoint_);
        if (n <= 0) break;

        bytesRcvd_ += static_cast<uint64_t>(n);
        lastRcvTimeTick_ = NowTickMS();

        // Track peer endpoint for server mode
        if (isServer_) {
            serverPeerCount_++;
            // Store endpoint for this peer
            uint32_t key = GetEndpointKey(peerEndpoint_);
            if (endpointToPlayerId_.find(key) == endpointToPlayerId_.end()) {
                uint32_t pid = nextPlayerId_++;
                playerIdToEndpoint_[pid] = peerEndpoint_;
                endpointToPlayerId_[key] = pid;
            }
        }

        // Parse framed message directly from UDP data
        uint16_t msgType = 0;
        std::vector<uint8_t> payload;
        if (Unframe(kcpInputBuf_.data(), static_cast<size_t>(n), msgType, payload)) {
            HandleIncomingFrame(kcpInputBuf_.data(), static_cast<size_t>(n), 0);
        }
    }
}

void NetworkLayer::ProcessKcpRcv() {
    // No KCP - this is a no-op
    (void)kcpInputBuf_;
}

void NetworkLayer::SendHeartbeat() {
    lastHeartbeatTick_ = NowTickMS();
    auto ping = std::make_unique<PingMessage>();
    ping->timestamp = lastHeartbeatTick_;
    ping->nonce = static_cast<uint32_t>(lastHeartbeatTick_ & 0xFFFFFFFF);
    Send(std::move(ping));
}

void NetworkLayer::CheckHeartbeatTimeout() {
    uint64_t now = NowTickMS();
    if (lastRcvTimeTick_ > 0) {
        float elapsed = static_cast<float>(now - lastRcvTimeTick_);
        if (elapsed > HEARTBEAT_TIMEOUT_S * 1000.0f) {
            connected_.store(false);
            if (onDisconnected_) {
                onDisconnected_();
            }
        }
    }
}

void NetworkLayer::AckReceived(uint32_t seq) {
    ackedSet_[seq] = NowTickMS();
    for (auto it = ackedSet_.begin(); it != ackedSet_.end();) {
        if (NowTickMS() - it->second > 5000) {
            it = ackedSet_.erase(it);
        } else {
            ++it;
        }
    }
}

void NetworkLayer::FlushRetransmits() {
    // No KCP retransmission needed
    (void)sendQueue_;
}

bool NetworkLayer::SendToPlayer(const std::vector<uint8_t>& frame, uint32_t playerId) {
    if (!udp_ || frame.empty() || !isServer_) return false;
    auto it = playerIdToEndpoint_.find(playerId);
    if (it != playerIdToEndpoint_.end()) {
        return udp_.SendTo(frame.data(), static_cast<int>(frame.size()), it->second) > 0;
    }
    if (!peerEndpoint_.port) return false;
    return udp_.SendTo(frame.data(), static_cast<int>(frame.size()), peerEndpoint_) > 0;
}

std::vector<uint32_t> NetworkLayer::GetAllClientIds() const {
    std::vector<uint32_t> ids;
    for (auto& [id, _] : playerIdToEndpoint_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<NetEndpoint> NetworkLayer::GetAllClientEndpoints() const {
    std::vector<NetEndpoint> eps;
    for (auto& [id, ep] : playerIdToEndpoint_) {
        eps.push_back(ep);
    }
    return eps;
}

uint32_t NetworkLayer::GetEndpointKey(const NetEndpoint& ep) const {
    return (static_cast<uint32_t>(ep.address[0]) << 24) |
           (static_cast<uint32_t>(ep.address[1]) << 16) |
           (static_cast<uint32_t>(ep.address[2]) << 8) |
           static_cast<uint32_t>(ep.address[3]);
}

ClientContext* NetworkLayer::GetOrCreateClientContext(const NetEndpoint& /*ep*/) {
    return nullptr;
}

ClientContext* NetworkLayer::GetClientContextByPlayerId(uint32_t /*playerId*/) const {
    return nullptr;
}

void NetworkLayer::HandleIncomingFrame(const uint8_t* data, size_t len, uint32_t /*playerId*/) {
    uint16_t msgType = 0;
    std::vector<uint8_t> payload;
    if (!Unframe(data, len, msgType, payload)) return;

    auto msg = MessageFactory::CreateMessage(msgType);
    if (!msg) return;

    const uint8_t* pData = payload.data();
    if (!msg->Deserialize(pData)) return;

    // Track RTT from ping/pong
    if (auto* ping = dynamic_cast<PingMessage*>(msg.get())) {
        if (ping->timestamp > 0 && ping->nonce > 0) {
            uint64_t sentTime = ping->timestamp;
            float rtt = static_cast<float>(NowTickMS() - sentTime);
            if (rtt > 0 && rttMs_ == 0) {
                rttMs_ = rtt;
            } else if (rttMs_ > 0) {
                rttMs_ = rttMs_ * 0.75f + rtt * 0.25f;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(recvMutex_);
        recvQueue_.push_back(std::move(msg));
    }
    msgsRcvd_++;

    if (onMessage_) {
        onMessage_(data, len);
    }
}

} // namespace PhysicsSync
