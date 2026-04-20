/**
 * @file network_protocol.h
 * @brief 网络协议定义 - 服务器-客户端通信的消息格式
 * 
 * 本文件定义了服务器和客户端之间通信的所有消息类型。
 * 每个消息都有唯一的类型ID，用于快速路由和处理。
 * 
 * 协议设计原则：
 * 1. 小包头 - 每个消息只有4字节的类型ID和长度
 * 2. 类型安全 - 使用枚举区分消息类型
 * 3. 扩展性 - 预留扩展字段
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace PhysicsSync {

// ================================================================
// 消息类型枚举
// ================================================================

/**
 * @brief 服务器→客户端消息类型
 */
enum class ServerMessageType : uint16_t {
    // 连接管理
    CONNECT_ACK = 1,        ///< 连接确认
    CONNECT_NACK = 2,       ///< 连接拒绝
    DISCONNECT = 3,         ///< 断开连接通知
    
    // 物理同步
    WORLD_SNAPSHOT = 10,    ///< 完整世界快照
    DELTA_UPDATE = 11,      ///< 增量更新
    PHYSICS_STATE = 12,     ///< 单个对象状态
    
    // 游戏逻辑
    GAME_STATE = 20,        ///< 游戏状态
    SCORE_UPDATE = 21,      ///< 分数更新
    
    // 系统
    PING = 30,              ///< 心跳包
    PONG = 31,              ///< 心跳响应
    ERROR_MSG = 32,         ///< 错误信息
};

/**
 * @brief 客户端→服务器消息类型
 */
enum class ClientMessageType : uint16_t {
    // 连接管理
    CONNECT_REQUEST = 1,    ///< 连接请求
    CONNECT_CHALLENGE = 2,  ///< 连接挑战响应
    
    // 物理输入
    PLAYER_INPUT = 10,      ///< 玩家输入
    INPUT_BURST = 11,       ///< 输入批处理
    
    // 游戏逻辑
    GAME_ACTION = 20,       ///< 游戏动作
    
    // 系统
    PING_REQUEST = 30,      ///< 心跳请求
};

// ================================================================
// 消息包头
// ================================================================

/**
 * @brief 网络消息包头
 * 
 * 每个网络消息都以这个固定大小的包头开头。
 * 大小：8字节（4字节类型 + 4字节数据长度）
 */
struct MessageHeader {
    uint16_t type;            ///< 消息类型ID
    uint16_t reserved;        ///< 保留字段（用于对齐）
    uint32_t dataLength;      ///< 消息数据长度（不包括包头）
    
    /** @brief 包头固定大小 */
    static constexpr size_t SIZE = 8;
    
    /** @brief 最大消息长度（64KB） */
    static constexpr uint32_t MAX_DATA_LENGTH = 65536;
    
    /**
     * @brief 验证包头是否有效
     * @return 如果有效返回true
     */
    bool IsValid() const {
        return dataLength <= MAX_DATA_LENGTH;
    }
};

// ================================================================
// 消息类
// ================================================================

/**
 * @brief 网络消息基类
 * 
 * 所有网络消息都继承自此类。
 * 提供序列化和反序列化功能。
 */
class NetworkMessage {
public:
    virtual ~NetworkMessage() = default;
    
    /**
     * @brief 获取消息类型
     * @return 消息类型
     */
    virtual uint16_t GetType() const = 0;
    
    /**
     * @brief 序列化为字节流
     * @param buffer 输出缓冲区
     */
    virtual void Serialize(std::vector<uint8_t>& buffer) const = 0;
    
    /**
     * @brief 从字节流反序列化
     * @param data 数据指针（会前进）
     * @return 是否成功
     */
    virtual bool Deserialize(const uint8_t*& data) = 0;
    
    /**
     * @brief 创建消息的克隆
     * @return 克隆的消息
     */
    virtual std::unique_ptr<NetworkMessage> Clone() const = 0;
};

/**
 * @brief 连接确认消息
 */
struct ConnectAckMessage : public NetworkMessage {
    uint32_t playerId;          ///< 分配的玩家ID
    uint32_t serverTick;        ///< 服务器当前tick
    float latency;              ///<  Estimated latency
    
    uint16_t GetType() const override { 
        return static_cast<uint16_t>(ServerMessageType::CONNECT_ACK); 
    }
    
    void Serialize(std::vector<uint8_t>& buffer) const override;
    bool Deserialize(const uint8_t*& data) override;
    std::unique_ptr<NetworkMessage> Clone() const override;
};

/**
 * @brief 世界快照消息
 */
struct WorldSnapshotMessage : public NetworkMessage {
    uint32_t snapshotId;        ///< 快照ID
    uint32_t tick;              ///< 快照对应的tick
    std::vector<uint8_t> stateData;  ///< 序列化后的物理状态数据
    mutable uint32_t dataSize;  ///< cached size for deserialization

    uint16_t GetType() const override { 
        return static_cast<uint16_t>(ServerMessageType::WORLD_SNAPSHOT); 
    }
    
    void Serialize(std::vector<uint8_t>& buffer) const override;
    bool Deserialize(const uint8_t*& data) override;
    std::unique_ptr<NetworkMessage> Clone() const override;
};

/**
 * @brief 玩家输入消息
 */
struct PlayerInputMessage : public NetworkMessage {
    uint32_t playerId;          ///< 玩家ID
    uint32_t tick;              ///< 输入对应的tick
    std::vector<uint8_t> inputData;  ///< 序列化后的输入数据
    
    uint16_t GetType() const override { 
        return static_cast<uint16_t>(ClientMessageType::PLAYER_INPUT); 
    }
    
    void Serialize(std::vector<uint8_t>& buffer) const override;
    bool Deserialize(const uint8_t*& data) override;
    std::unique_ptr<NetworkMessage> Clone() const override;
};

/**
 * @brief Ping消息
 */
struct PingMessage : public NetworkMessage {
    uint64_t timestamp;         ///< 发送时间戳
    uint32_t nonce;             ///< 随机数（用于匹配请求/响应）
    
    uint16_t GetType() const override;  // 可以是PING或PONG
    
    void Serialize(std::vector<uint8_t>& buffer) const override;
    bool Deserialize(const uint8_t*& data) override;
    std::unique_ptr<NetworkMessage> Clone() const override;
};

/**
 * @brief 错误消息
 */
struct ErrorMessage : public NetworkMessage {
    uint32_t errorCode;         ///< 错误码
    std::string message;        ///< 错误描述
    
    uint16_t GetType() const override { 
        return static_cast<uint16_t>(ServerMessageType::ERROR_MSG); 
    }
    
    void Serialize(std::vector<uint8_t>& buffer) const override;
    bool Deserialize(const uint8_t*& data) override;
    std::unique_ptr<NetworkMessage> Clone() const override;
};

// ================================================================
// 消息工厂
// ================================================================

/**
 * @brief 消息工厂 - 根据类型创建消息
 */
class MessageFactory {
public:
    /**
     * @brief 根据类型创建消息
     * @param type 消息类型
     * @return 消息指针（如果类型未知返回nullptr）
     */
    static std::unique_ptr<NetworkMessage> CreateMessage(uint16_t type);
    
    /**
     * @brief 注册自定义消息类型
     * @param type 消息类型
     * @param factory 工厂函数
     */
    static void RegisterFactory(uint16_t type, 
        std::function<std::unique_ptr<NetworkMessage>()> factory);
};

// ================================================================
// 网络连接接口
// ================================================================

/**
 * @brief 网络连接接口
 * 
 * 所有网络连接必须实现此接口。
 * 这使得我们可以轻松切换底层传输层（KCP、TCP、WebSocket等）。
 */
class NetworkConnection {
public:
    virtual ~NetworkConnection() = default;
    
    /**
     * @brief 发送消息
     * @param message 要发送的消息
     * @return 是否成功
     */
    virtual bool Send(const NetworkMessage& message) = 0;
    
    /**
     * @brief 接收消息
     * @return 接收到的消息（如果没有则返回nullptr）
     */
    virtual std::unique_ptr<NetworkMessage> Receive() = 0;
    
    /**
     * @brief 检查连接是否存活
     * @return 如果连接正常返回true
     */
    virtual bool IsAlive() const = 0;
    
    /**
     * @brief 获取延迟（毫秒）
     * @return 往返延迟
     */
    virtual float GetLatency() const = 0;
    
    /**
     * @brief 关闭连接
     */
    virtual void Close() = 0;
};

} // namespace PhysicsSync
