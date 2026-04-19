/**
 * @file serializer.cpp
 * @brief 二进制序列化/反序列化工具类实现
 * 
 * 提供高效的二进制序列化功能，用于网络传输和状态保存。
 * 使用网络字节序（大端序）以确保跨平台兼容性。
 */

#include "serializer.h"

namespace PhysicsSync {

// =====================================================================
// 网络字节序转换（大端序）
// =====================================================================

/**
 * @brief 将主机字节序转换为网络字节序（16位）
 */
uint16_t Serializer::HostToNetwork(uint16_t value) {
    return (value << 8) | (value >> 8);
}

/**
 * @brief 将主机字节序转换为网络字节序（32位）
 */
uint32_t Serializer::HostToNetwork(uint32_t value) {
    return ((value & 0xFF000000u) >> 24) |
           ((value & 0x00FF0000u) >> 8)  |
           ((value & 0x0000FF00u) << 8)  |
           ((value & 0x000000FFu) << 24);
}

/**
 * @brief 将主机字节序转换为网络字节序（64位）
 */
uint64_t Serializer::HostToNetwork(uint64_t value) {
    return static_cast<uint64_t>(HostToNetwork(static_cast<uint32_t>(value >> 32))) |
           (static_cast<uint64_t>(HostToNetwork(static_cast<uint32_t>(value & 0xFFFFFFFFu))) << 32);
}

// =====================================================================
// Serializer 实现
// =====================================================================

Serializer::Serializer(size_t bufferSize)
    : buffer_(bufferSize, 0)
    , bytesWritten_(0)
{
}

bool Serializer::WriteByte(uint8_t byte) {
    if (bytesWritten_ >= buffer_.size()) {
        // 自动扩展缓冲区
        buffer_.resize(buffer_.size() * 2, 0);
    }
    buffer_[bytesWritten_++] = byte;
    return true;
}

bool Serializer::Serialize(uint8_t value) {
    return WriteByte(value);
}

bool Serializer::Serialize(uint16_t value) {
    value = HostToNetwork(value);
    return SerializeRaw(&value, sizeof(uint16_t));
}

bool Serializer::Serialize(uint32_t value) {
    value = HostToNetwork(value);
    return SerializeRaw(&value, sizeof(uint32_t));
}

bool Serializer::Serialize(uint64_t value) {
    value = HostToNetwork(value);
    return SerializeRaw(&value, sizeof(uint64_t));
}

bool Serializer::Serialize(float value) {
    // 直接复制浮点数的字节表示
    uint32_t intRep;
    std::memcpy(&intRep, &value, sizeof(float));
    intRep = HostToNetwork(intRep);
    std::memcpy(&value, &intRep, sizeof(float));
    return SerializeRaw(&value, sizeof(float));
}

bool Serializer::Serialize(double value) {
    // 将double转换为float再序列化（为了节省带宽）
    float fValue = static_cast<float>(value);
    return Serialize(fValue);
}

bool Serializer::Serialize(bool value) {
    uint8_t byte = value ? 1 : 0;
    return WriteByte(byte);
}

bool Serializer::Serialize(const std::string& str) {
    // 序列化字符串长度
    uint32_t length = static_cast<uint32_t>(str.size());
    if (!Serialize(length)) {
        return false;
    }
    // 序列化字符串内容
    if (length > 0) {
        return SerializeRaw(str.data(), length);
    }
    return true;
}

bool Serializer::SerializeRaw(const void* data, size_t size) {
    if (bytesWritten_ + size > buffer_.size()) {
        buffer_.resize(bytesWritten_ + size, 0);
    }
    std::memcpy(buffer_.data() + bytesWritten_, data, size);
    bytesWritten_ += size;
    return true;
}

void Serializer::Reset() {
    bytesWritten_ = 0;
    buffer_.assign(buffer_.size(), 0);
}

void Serializer::Reserve(size_t additionalSize) {
    buffer_.reserve(bytesWritten_ + additionalSize);
}

// =====================================================================
// Deserializer 实现
// =====================================================================

Deserializer::Deserializer(const std::vector<uint8_t>& data)
    : data_(data.data())
    , dataSize_(data.size())
    , position_(0)
{
}

Deserializer::Deserializer(const uint8_t* data, size_t size)
    : data_(data)
    , dataSize_(size)
    , position_(0)
{
}

uint16_t Deserializer::NetworkToHost(uint16_t value) {
    return (value << 8) | (value >> 8);
}

uint32_t Deserializer::NetworkToHost(uint32_t value) {
    return ((value & 0xFF000000u) >> 24) |
           ((value & 0x00FF0000u) >> 8)  |
           ((value & 0x0000FF00u) << 8)  |
           ((value & 0x000000FFu) << 24);
}

uint64_t Deserializer::NetworkToHost(uint64_t value) {
    return static_cast<uint64_t>(NetworkToHost(static_cast<uint32_t>(value >> 32))) |
           (static_cast<uint64_t>(NetworkToHost(static_cast<uint32_t>(value & 0xFFFFFFFFu))) << 32);
}

bool Deserializer::ReadByte(uint8_t& value) {
    if (position_ >= dataSize_) {
        return false;
    }
    value = data_[position_++];
    return true;
}

bool Deserializer::Deserialize(uint8_t& value) {
    return ReadByte(value);
}

bool Deserializer::Deserialize(uint16_t& value) {
    if (position_ + sizeof(uint16_t) > dataSize_) {
        return false;
    }
    std::memcpy(&value, data_ + position_, sizeof(uint16_t));
    value = NetworkToHost(value);
    position_ += sizeof(uint16_t);
    return true;
}

bool Deserializer::Deserialize(uint32_t& value) {
    if (position_ + sizeof(uint32_t) > dataSize_) {
        return false;
    }
    std::memcpy(&value, data_ + position_, sizeof(uint32_t));
    value = NetworkToHost(value);
    position_ += sizeof(uint32_t);
    return true;
}

bool Deserializer::Deserialize(uint64_t& value) {
    if (position_ + sizeof(uint64_t) > dataSize_) {
        return false;
    }
    std::memcpy(&value, data_ + position_, sizeof(uint64_t));
    value = NetworkToHost(value);
    position_ += sizeof(uint64_t);
    return true;
}

bool Deserializer::Deserialize(float& value) {
    if (position_ + sizeof(float) > dataSize_) {
        return false;
    }
    uint32_t intRep;
    std::memcpy(&intRep, data_ + position_, sizeof(float));
    intRep = NetworkToHost(intRep);
    std::memcpy(&value, &intRep, sizeof(float));
    position_ += sizeof(float);
    return true;
}

bool Deserializer::Deserialize(double& value) {
    float fValue;
    if (!Deserialize(fValue)) {
        return false;
    }
    value = static_cast<double>(fValue);
    return true;
}

bool Deserializer::Deserialize(bool& value) {
    uint8_t byte;
    if (!ReadByte(byte)) {
        return false;
    }
    value = (byte != 0);
    return true;
}

bool Deserializer::Deserialize(std::string& str) {
    uint32_t length;
    if (!Deserialize(length)) {
        return false;
    }
    if (position_ + length > dataSize_) {
        return false;
    }
    str.assign(reinterpret_cast<const char*>(data_ + position_), length);
    position_ += length;
    return true;
}

bool Deserializer::DeserializeRaw(void* output, size_t size) {
    if (position_ + size > dataSize_) {
        return false;
    }
    std::memcpy(output, data_ + position_, size);
    position_ += size;
    return true;
}

size_t Deserializer::GetBytesRemaining() const {
    if (position_ >= dataSize_) {
        return 0;
    }
    return dataSize_ - position_;
}

bool Deserializer::IsEOF() const {
    return position_ >= dataSize_;
}

void Deserializer::Reset() {
    position_ = 0;
}

bool Deserializer::Seek(size_t position) {
    if (position > dataSize_) {
        return false;
    }
    position_ = position;
    return true;
}

} // namespace PhysicsSync
