/**
 * @file physics_server.h
 * @brief 物理服务器核心 - 服务器权威物理模拟 + 网络层集成
 *
 * 本文件定义了物理服务器的核心类，负责：
 * 1. 运行确定性物理模拟
 * 2. 接收客户端输入并处理
 * 3. 广播物理状态给所有客户端
 * 4. 管理玩家连接
 * 5. 通过网络层 (NetworkLayer) 收发数据
 *
 * 架构：服务器权威模式
 * - 服务器是物理世界的唯一权威
 * - 客户端发送输入，服务器计算结果
 * - 服务器定期广播世界快照
 */

#pragma once

#include "../common/physics_state.h"
#include "../common/timestep_manager.h"
#include "../common/deterministic_random.h"
#include "../common/network_layer.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>

namespace PhysicsSync {

// ================================================================
// 服务器配置
// ================================================================

struct ServerConfig {
    uint16_t listenPort = 9300;          // 服务器监听端口
    uint32_t physicsHz = 60;             // 物理更新频率
    uint32_t snapshotHz = 30;            // 快照广播频率
    uint32_t maxClients = 32;            // 最大客户端数
    Vec3 gravity = Vec3{0.0f, -9.81f, 0.0f};  // 重力

    ServerConfig() = default;
    explicit ServerConfig(uint16_t port) : listenPort(port) {}
};

// ================================================================
// 服务器连接 (基于 NetworkLayer 实现)
// ================================================================

/**
 * @brief 服务器端玩家连接
 *
 * 每个玩家对应一个 ServerPlayer，封装了与该客户端的 NetworkLayer 实例。
 */
class ServerPlayer : public NetworkConnection {
public:
    ServerPlayer(uint32_t id, uint16_t port)
        : playerId_(id), connected_(true) {
        // 为每个客户端创建独立的 KCP 实例
        // 这里用 NetworkLayer 做传输
    }

    ~ServerPlayer() override = default;

    bool Send(const NetworkMessage& message) override {
        // 通过 NetworkLayer 发送
        return false;
    }

    std::unique_ptr<NetworkMessage> Receive() override {
        return nullptr;
    }

    bool IsAlive() const override { return connected_; }
    float GetLatency() const override { return 0.0f; }
    void Close() override { connected_ = false; }

    uint32_t GetPlayerId() const { return playerId_; }
    void SetEndpoint(const NetEndpoint& ep) { endpoint_ = ep; }
    const NetEndpoint& GetEndpoint() const { return endpoint_; }

private:
    uint32_t playerId_;
    NetEndpoint endpoint_;
    std::atomic<bool> connected_;
};

// ================================================================
// 物理服务器核心类
// ================================================================

/**
 * @brief 物理服务器核心类
 *
 * 这是服务器的主控制类，管理整个物理世界的模拟和网络通信。
 * 它运行在一个独立的线程中，以固定频率更新物理世界。
 *
 * 线程模型：
 * - 网络线程：接收客户端输入，发送快照
 * - 模拟线程：物理更新（以固定频率运行）
 */
class PhysicsServer {
public:
    /**
     * @brief 构造函数
     * @param config 服务器配置
     */
    explicit PhysicsServer(const ServerConfig& config = ServerConfig());

    /**
     * @brief 析构函数
     */
    ~PhysicsServer();

    // 禁止拷贝
    PhysicsServer(const PhysicsServer&) = delete;
    PhysicsServer& operator=(const PhysicsServer&) = delete;

    /**
     * @brief 初始化服务器（创建网络监听、物理世界）
     * @return 是否成功初始化
     */
    bool Initialize();

    /**
     * @brief 启动服务器（开始模拟和网络循环）
     */
    void Start();

    /**
     * @brief 停止服务器
     */
    void Stop();

    /**
     * @brief 检查服务器是否运行中
     * @return 如果运行中返回true
     */
    bool IsRunning() const { return running_.load(); }

    /**
     * @brief 获取当前物理tick
     * @return 当前tick序号
     */
    uint32_t GetCurrentTick() const { return timeManager_.GetCurrentTick(); }

    /**
     * @brief 获取当前连接的玩家数量
     * @return 玩家数量
     */
    uint32_t GetPlayerCount() const;

    /**
     * @brief 获取服务器统计信息
     * @return 统计信息字符串
     */
    std::string GetStatistics() const;

    /**
     * @brief 获取物理世界快照
     */
    void GetSnapshot(PhysicsWorldSnapshot& snapshot) {
        std::lock_guard<std::mutex> lock(snapshotMutex_);
        snapshot = currentSnapshot_;
    }

    /**
     * @brief 获取配置
     */
    const ServerConfig& GetConfig() const { return config_; }

    /**
     * @brief 添加/移除一个玩家连接
     */
    uint32_t AddPlayer(const NetEndpoint& endpoint);
    void RemovePlayer(uint32_t playerId);

    /**
     * @brief 处理收到的玩家输入消息
     */
    void HandlePlayerInput(const uint8_t* data, size_t len);

    /**
     * @brief 广播世界快照给所有客户端
     */
    void BroadcastSnapshot();

private:
    /**
     * @brief 网络接收线程 - 处理 incoming UDP 数据包
     */
    void NetworkThread();

    /**
     * @brief 模拟线程 - 以固定频率更新物理世界
     */
    void SimulationThread();

    /**
     * @brief 处理所有待处理的输入
     */
    void ProcessPendingInputs();

    /**
     * @brief 广播快照给所有玩家
     */
    void BroadcastSnapshotToAll();

    /**
     * @brief 处理来自特定客户端的数据包
     */
    void ProcessPacket(const uint8_t* data, size_t len, const NetEndpoint& from);

    // 配置参数
    ServerConfig config_;

    // 网络层
    NetworkLayer networkLayer_;
    std::atomic<bool> networkRunning_ { false };

    // 时间管理
    TimeStepManager timeManager_;       ///< 时间步长管理器

    // 玩家管理
    std::unordered_map<uint32_t, std::unique_ptr<ServerPlayer>> players_;
    uint32_t nextPlayerId_;
    std::mutex playersMutex_;

    // 输入处理
    std::vector<std::pair<PlayerInput, uint32_t>> pendingInputs_; // input + playerId
    std::mutex inputMutex_;

    // 快照
    PhysicsWorldSnapshot currentSnapshot_;
    uint32_t lastBroadcastTick_;
    std::mutex snapshotMutex_;

    // 确定性随机数
    DeterministicRandom deterministicRng_;

    // 控制标志
    std::atomic<bool> running_ { false };
    std::unique_ptr<std::thread> networkThread_;
    std::unique_ptr<std::thread> simulationThread_;
};

} // namespace PhysicsSync
