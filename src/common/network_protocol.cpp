/**
 * @file network_protocol.cpp
 * @brief Network message serialization/deserialization implementations
 */

#include "network_protocol.h"

namespace PhysicsSync {

// ================================================================
// ConnectAckMessage
// ================================================================

void ConnectAckMessage::Serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&playerId),
                  reinterpret_cast<const uint8_t*>(&playerId) + sizeof(playerId));
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&serverTick),
                  reinterpret_cast<const uint8_t*>(&serverTick) + sizeof(serverTick));
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&latency),
                  reinterpret_cast<const uint8_t*>(&latency) + sizeof(latency));
}

bool ConnectAckMessage::Deserialize(const uint8_t*& data) {
    std::memcpy(&playerId, data, sizeof(playerId)); data += sizeof(playerId);
    std::memcpy(&serverTick, data, sizeof(serverTick)); data += sizeof(serverTick);
    std::memcpy(&latency, data, sizeof(latency)); data += sizeof(latency);
    return true;
}

std::unique_ptr<NetworkMessage> ConnectAckMessage::Clone() const {
    return std::make_unique<ConnectAckMessage>(*this);
}

// ================================================================
// WorldSnapshotMessage
// ================================================================

void WorldSnapshotMessage::Serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&snapshotId),
                  reinterpret_cast<const uint8_t*>(&snapshotId) + sizeof(snapshotId));
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&tick),
                  reinterpret_cast<const uint8_t*>(&tick) + sizeof(tick));
    uint32_t dataSize = static_cast<uint32_t>(stateData.size());
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&dataSize),
                  reinterpret_cast<const uint8_t*>(&dataSize) + sizeof(dataSize));
    buffer.insert(buffer.end(), stateData.begin(), stateData.end());
}

bool WorldSnapshotMessage::Deserialize(const uint8_t*& data) {
    std::memcpy(&snapshotId, data, sizeof(snapshotId)); data += sizeof(snapshotId);
    std::memcpy(&tick, data, sizeof(tick)); data += sizeof(tick);
    uint32_t dataSize = 0;
    std::memcpy(&dataSize, data, sizeof(dataSize)); data += sizeof(dataSize);
    if (data + dataSize > data) {
        // safety check
    }
    stateData.assign(data, data + dataSize);
    data += dataSize;
    return true;
}

std::unique_ptr<NetworkMessage> WorldSnapshotMessage::Clone() const {
    return std::make_unique<WorldSnapshotMessage>(*this);
}

// ================================================================
// PlayerInputMessage
// ================================================================

void PlayerInputMessage::Serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&playerId),
                  reinterpret_cast<const uint8_t*>(&playerId) + sizeof(playerId));
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&tick),
                  reinterpret_cast<const uint8_t*>(&tick) + sizeof(tick));
    uint32_t dataSize = static_cast<uint32_t>(inputData.size());
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&dataSize),
                  reinterpret_cast<const uint8_t*>(&dataSize) + sizeof(dataSize));
    buffer.insert(buffer.end(), inputData.begin(), inputData.end());
}

bool PlayerInputMessage::Deserialize(const uint8_t*& data) {
    std::memcpy(&playerId, data, sizeof(playerId)); data += sizeof(playerId);
    std::memcpy(&tick, data, sizeof(tick)); data += sizeof(tick);
    uint32_t dataSize = 0;
    std::memcpy(&dataSize, data, sizeof(dataSize)); data += sizeof(dataSize);
    inputData.assign(data, data + dataSize);
    data += dataSize;
    return true;
}

std::unique_ptr<NetworkMessage> PlayerInputMessage::Clone() const {
    return std::make_unique<PlayerInputMessage>(*this);
}

// ================================================================
// PingMessage
// ================================================================

uint16_t PingMessage::GetType() const {
    // Distinguished by context; this constructor creates a request
    return static_cast<uint16_t>(ClientMessageType::PING_REQUEST);
}

void PingMessage::Serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&timestamp),
                  reinterpret_cast<const uint8_t*>(&timestamp) + sizeof(timestamp));
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&nonce),
                  reinterpret_cast<const uint8_t*>(&nonce) + sizeof(nonce));
}

bool PingMessage::Deserialize(const uint8_t*& data) {
    std::memcpy(&timestamp, data, sizeof(timestamp)); data += sizeof(timestamp);
    std::memcpy(&nonce, data, sizeof(nonce)); data += sizeof(nonce);
    return true;
}

std::unique_ptr<NetworkMessage> PingMessage::Clone() const {
    return std::make_unique<PingMessage>(*this);
}

// ================================================================
// ErrorMessage
// ================================================================

void ErrorMessage::Serialize(std::vector<uint8_t>& buffer) const {
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&errorCode),
                  reinterpret_cast<const uint8_t*>(&errorCode) + sizeof(errorCode));
    uint32_t msgLen = static_cast<uint32_t>(message.size());
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&msgLen),
                  reinterpret_cast<const uint8_t*>(&msgLen) + sizeof(msgLen));
    buffer.insert(buffer.end(), message.begin(), message.end());
}

bool ErrorMessage::Deserialize(const uint8_t*& data) {
    std::memcpy(&errorCode, data, sizeof(errorCode)); data += sizeof(errorCode);
    uint32_t msgLen = 0;
    std::memcpy(&msgLen, data, sizeof(msgLen)); data += sizeof(msgLen);
    message.assign(data, data + msgLen);
    data += msgLen;
    return true;
}

std::unique_ptr<NetworkMessage> ErrorMessage::Clone() const {
    return std::make_unique<ErrorMessage>(*this);
}

// ================================================================
// MessageFactory
// ================================================================

std::unique_ptr<NetworkMessage> MessageFactory::CreateMessage(uint16_t type) {
    // Server messages
    switch (static_cast<ServerMessageType>(type)) {
        case ServerMessageType::CONNECT_ACK:
            return std::make_unique<ConnectAckMessage>();
        case ServerMessageType::WORLD_SNAPSHOT:
            return std::make_unique<WorldSnapshotMessage>();
        case ServerMessageType::PING:
            return std::make_unique<PingMessage>();
        case ServerMessageType::PONG:
            return std::make_unique<PingMessage>();
        case ServerMessageType::ERROR_MSG:
            return std::make_unique<ErrorMessage>();
        default:
            break;
    }

    // Client messages
    switch (static_cast<ClientMessageType>(type)) {
        case ClientMessageType::CONNECT_REQUEST:
            return std::make_unique<ConnectAckMessage>();  // placeholder
        case ClientMessageType::PLAYER_INPUT:
            return std::make_unique<PlayerInputMessage>();
        case ClientMessageType::PING_REQUEST:
            return std::make_unique<PingMessage>();
        default:
            break;
    }

    return nullptr;
}

void MessageFactory::RegisterFactory(uint16_t /*type*/,
    std::function<std::unique_ptr<NetworkMessage>()> /*factory*/) {
    // Extensibility point for custom messages
}

} // namespace PhysicsSync
