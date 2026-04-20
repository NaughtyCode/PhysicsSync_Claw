/**
 * @file network_layer.cpp
 * @brief Reliable network transport over UDP + KCP
 *
 * Implements message framing, sequencing, retransmission, heartbeat,
 * and a clean API for server and client usage.
 */

#include "network_layer.h"
#include <sstream>
#include <cstring>
#include <chrono>

namespace PhysicsSync {

// -- Helpers --
static uint64_t NowTickMS() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

// -- IP address string -> NetEndpoint --
static bool ParseEndpoint(const char* addrStr, uint16_t port, NetEndpoint* out) {
    if (!out) return false;
    // Support dotted-decimal or "127.0.0.1" style
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
    // Try "any" or empty
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

// -- Frame building --

bool NetworkLayer::BuildFrame(uint16_t msgType, const uint8_t* payload,
                               size_t payloadLen, std::vector<uint8_t>& frame) {
    frame.clear();
    frame.reserve(FRAME_HEADER_SIZE + payloadLen);
    // Header: type (2) + payload size (2)
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

// -- KCP output callback --
// (called by KCP when it has data to send over UDP)

int NetworkLayer::OnKCPOutput(const char* buf, int len) {
    SendUDP(reinterpret_cast<const uint8_t*>(buf), len);
    return 0;
}

// -- Server creation --

bool NetworkLayer::CreateAsServer(const char* addr, uint16_t port) {
    isServer_ = true;
    if (!udp_.Create()) {
        return false;
    }
    NetEndpoint bindAddr = NetEndpoint::Any(port);
    if (addr && strcmp(addr, "0.0.0.0") != 0) {
        ParseEndpoint(addr, port, &bindAddr);
    }
    if (!udp_.Bind(bindAddr)) {
        return false;
    }
    udp_.SetNonBlocking(true);
    connected_.store(true);
    serverPeerCount_ = 0;
    return true;
}

// -- Client creation --

bool NetworkLayer::CreateAsClient(const char* addr, uint16_t port) {
    isServer_ = false;
    if (!udp_.Create()) {
        return false;
    }
    udp_.SetNonBlocking(true);
    if (!ParseEndpoint(addr, port, &serverEndpoint_)) {
        return false;
    }
    return true;
}

// -- Client connect --
// For UDP+KCP "connect" just means initial connection handshake.

bool NetworkLayer::Connect() {
    if (isServer_) return false;

    // Send a CONNECT_REQUEST immediately
    auto msg = std::make_unique<PlayerInputMessage>();
    PlayerInputMessage* cm = static_cast<PlayerInputMessage*>(msg.get());
    cm->playerId = 0;
    cm->tick = 0;
    // Empty inputData for connect request
    Send(std::move(msg));

    return true;
}

// -- Main update loop --

void NetworkLayer::Update() {
    if (!udp_ || closing_.load()) return;

    uint64_t now = NowTickMS();

    // 1. Receive UDP datagrams and feed into KCP
    ProcessUDPRcv();

    // 2. KCP update
    if (kcp_) {
        kcp_->Update(static_cast<uint32_t>(now));
    }

    // 3. Flush retransmissions
    FlushRetransmits();

    // 4. Heartbeat
    if (now - lastHeartbeatTick_ >= HEARTBEAT_INTERVAL_MS) {
        SendHeartbeat();
    }
    CheckHeartbeatTimeout();
}

// -- Send a NetworkMessage --

bool NetworkLayer::Send(std::unique_ptr<NetworkMessage> msg) {
    if (!msg || !connected_.load()) return false;

    // Serialize payload
    std::vector<uint8_t> payload;
    msg->Serialize(payload);

    // Build frame with message type
    std::vector<uint8_t> frame;
    BuildFrame(msg->GetType(), payload.data(), payload.size(), frame);

    // Wrap in OutgoingMessage for sequencing
    OutgoingMessage om;
    om.type = msg->GetType();
    om.payload = std::move(frame);
    om.seq = nextSeqOut_++;
    om.sentAt = NowTickMS();
    om.retries = 0;
    sendQueue_.push_back(std::move(om));

    msgsSent_++;

    // Immediately try to send (KCP will buffer/split as needed)
    if (kcp_) {
        int rc = kcp_->Send(om.payload.data(), static_cast<int>(om.payload.size()));
        if (rc < 0) {
            // Will be retried on next Update
        }
    }

    return true;
}

// -- Receive (non-blocking) --

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

    if (kcp_) {
        kcp_.reset();
    }
    udp_.Close();

    sendQueue_.clear();
    {
        std::lock_guard<std::mutex> lock(recvMutex_);
        recvQueue_.clear();
    }
    ackedSet_.clear();

    closing_.store(false);
}

// -- Statistics --

std::string NetworkLayer::GetStats() const {
    std::ostringstream oss;
    oss << "NetworkLayer{"
        << "connected=" << connected_.load()
        << ", rtt=" << rttMs_ << "ms"
        << ", sent=" << msgsSent_
        << ", rcvd=" << msgsRcvd_
        << ", bytes_sent=" << bytesSent_
        << ", bytes_rcvd=" << bytesRcvd_
        << ", retransmits=" << retransmits_
        << ", queue=" << sendQueue_.size()
        << "}";
    return oss.str();
}

// -- Internal helpers --

void NetworkLayer::SendUDP(const uint8_t* data, int size) {
    if (!udp_ || size <= 0) return;

    if (isServer_) {
        // Server sends to last-known peer
        if (!peerEndpoint_.port) return;
    }

    int sent = udp_.SendTo(data, size,
        isServer_ ? peerEndpoint_ : serverEndpoint_);
    if (sent > 0) {
        bytesSent_ += static_cast<uint64_t>(sent);
    }
}

void NetworkLayer::ProcessUDPRcv() {
    while (udp_.HasData()) {
        int n = udp_.ReceiveFrom(kcpInputBuf_.data(),
                                  static_cast<int>(kcpInputBuf_.size()),
                                  &peerEndpoint_);
        if (n <= 0) break;

        bytesRcvd_ += static_cast<uint64_t>(n);
        lastRcvTimeTick_ = NowTickMS();

        // Feed raw UDP data into KCP
        if (kcp_) {
            kcp_->Input(kcpInputBuf_.data(), n);
        }

        // Also try to parse as a framed message for direct delivery
        // (KCP deduplicates; this is for fast-path handling)
        uint16_t msgType = 0;
        std::vector<uint8_t> payload;
        if (Unframe(kcpInputBuf_.data(), static_cast<size_t>(n), msgType, payload)) {
            HandleIncomingFrame(kcpInputBuf_.data(), static_cast<size_t>(n));
        }
    }
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
            // Connection timed out
            connected_.store(false);
            if (onDisconnected_) {
                onDisconnected_();
            }
        }
    }
}

void NetworkLayer::AckReceived(uint32_t seq) {
    ackedSet_[seq] = NowTickMS();
    // Prune old acks
    for (auto it = ackedSet_.begin(); it != ackedSet_.end();) {
        if (NowTickMS() - it->second > 5000) {
            it = ackedSet_.erase(it);
        } else {
            ++it;
        }
    }
}

void NetworkLayer::FlushRetransmits() {
    uint64_t now = NowTickMS();
    for (auto it = sendQueue_.begin(); it != sendQueue_.end();) {
        if (now - it->sentAt >= 200) { // 200ms retransmit interval
            if (it->retries >= 3) {
                // Too many retries, give up
                packetsDropped_++;
                it = sendQueue_.erase(it);
                continue;
            }
            if (kcp_) {
                kcp_->Send(it->payload.data(), static_cast<int>(it->payload.size()));
            }
            it->sentAt = now;
            it->retries++;
            retransmits_++;
        } else {
            ++it;
        }
    }
}

void NetworkLayer::BuildServerConnectACK(uint32_t playerId) {
    auto msg = std::make_unique<ConnectAckMessage>();
    ConnectAckMessage* cm = static_cast<ConnectAckMessage*>(msg.get());
    cm->playerId = playerId;
    cm->serverTick = static_cast<uint32_t>(NowTickMS());
    cm->latency = rttMs_;
    Send(std::move(msg));
}

bool NetworkLayer::SendToPlayer(const std::vector<uint8_t>& frame, uint32_t playerId) {
    if (!udp_ || frame.empty() || !isServer_) return false;

    // Look up the endpoint for this playerId
    auto it = playerIdToEndpoint_.find(playerId);
    if (it == playerIdToEndpoint_.end()) {
        // Fallback: use last known peer
        if (!peerEndpoint_.port) return false;
    } else {
        return udp_.SendTo(frame.data(), static_cast<int>(frame.size()), it->second) > 0;
    }

    return udp_.SendTo(frame.data(), static_cast<int>(frame.size()), peerEndpoint_) > 0;
}

void NetworkLayer::HandleIncomingFrame(const uint8_t* data, size_t len) {
    uint16_t msgType = 0;
    std::vector<uint8_t> payload;
    if (!Unframe(data, len, msgType, payload)) return;

    // Parse the message
    auto msg = MessageFactory::CreateMessage(msgType);
    if (!msg) {
        // Unknown message type, skip
        return;
    }

    const uint8_t* pData = payload.data();
    if (!msg->Deserialize(pData)) {
        return;
    }

    // Track RTT from ping/pong
    if (auto* ping = dynamic_cast<PingMessage*>(msg.get())) {
        // If this is a pong response (check nonce/timestamp), measure RTT
        if (ping->timestamp > 0 && ping->nonce > 0) {
            uint64_t sentTime = ping->timestamp;
            float rtt = static_cast<float>(NowTickMS() - sentTime);
            if (rtt > 0 && rttMs_ == 0) {
                rttMs_ = rtt;
            } else if (rttMs_ > 0) {
                // Smoothed average
                rttMs_ = rttMs_ * 0.75f + rtt * 0.25f;
            }
        }
    }

    // Enqueue for Receive()
    {
        std::lock_guard<std::mutex> lock(recvMutex_);
        recvQueue_.push_back(std::move(msg));
    }
    msgsRcvd_++;

    // Invoke callback
    if (onMessage_) {
        onMessage_(data, len);
    }
}

} // namespace PhysicsSync
