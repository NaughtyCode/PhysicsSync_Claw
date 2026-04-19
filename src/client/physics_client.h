/**
 * @file physics_client.h
 * @brief 物理客户端 - 客户端预测与校正 + 网络层集成
 *
 * 本文件定义了物理客户端的核心类，负责：
 * 1. 接收服务器的物理状态快照
 * 2. 客户端预测（基于收到的状态预测未来位置）
 * 3. 状态校正（当收到服务器权威状态时进行校正）
 * 4. 渲染插值（平滑显示远程物体）
 * 5. 通过网络层 (NetworkLayer) 收发数据
 *
 * 架构：客户端预测 + 服务器校正
 * - 客户端本地运行物理模拟，提供即时反馈
 * - 收到服务器快照后，校正本地状态
 * - 使用插值使远程物体运动平滑
 */

#pragma once

#include "../common/physics_state.h"
#include "../common/timestep_manager.h"
#include "../common/deterministic_random.h"
#include "../common/network_layer.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <deque>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>

namespace PhysicsSync {

// ================================================================
// 客户端状态
// ================================================================

enum class ClientState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    SYNCING,
    RUNNING,
    RECONNECTING,
};

enum class CorrectionMode {
    NONE,
    LERP,
    SNAP,
    ROLLBACK,
};

// ================================================================
// 渲染回调
// ================================================================

using render_callback_t = std::function<void(uint32_t timestamp, float interpolationAlpha)>;

// ================================================================
// 客户端配置
// ================================================================

struct ClientConfig {
    std::string serverHost = "127.0.0.1";  // 服务器地址
    uint16_t serverPort = 9300;             // 服务器端口
    float snapshotInterval = 1.0f / 30.0f;  // 快照间隔 (秒)
    float predictionWindow = 0.1f;          // 预测窗口 (秒)
    uint32_t physicsHz = 60;                // 本地物理频率
    CorrectionMode defaultCorrection = CorrectionMode::LERP;

    ClientConfig() = default;
    ClientConfig(const std::string& host, uint16_t port)
        : serverHost(host), serverPort(port) {}
};

// ================================================================
// 物理客户端
// ================================================================

class PhysicsClient {
public:
    /**
     * @brief 构造函数
     * @param config 客户端配置
     * @param renderCallback 渲染回调
     */
    PhysicsClient(const ClientConfig& config = ClientConfig(),
                  render_callback_t renderCallback = nullptr);

    ~PhysicsClient();

    PhysicsClient(const PhysicsClient&) = delete;
    PhysicsClient& operator=(const PhysicsClient&) = delete;

    /**
     * @brief 初始化客户端
     */
    bool Initialize();

    /**
     * @brief 连接到服务器
     */
    bool Connect();

    /**
     * @brief 断开连接
     */
    void Disconnect();

    /**
     * @brief 启动客户端（开始网络循环和模拟）
     */
    void Start();

    /**
     * @brief 停止客户端
     */
    void Stop();

    bool IsRunning() const { return running_.load(); }
    ClientState GetState() const { return state_; }
    bool IsConnected() const { return state_ == ClientState::CONNECTED ||
                                       state_ == ClientState::RUNNING; }

    /**
     * @brief 发送玩家输入
     */
    bool SendInput(const PlayerInput& input);

    /**
     * @brief 处理收到的服务器快照
     */
    bool ProcessSnapshot(const PhysicsWorldSnapshot& snapshot);

    bool GetWorldState(PhysicsWorldSnapshot& output);
    void SetCorrectionMode(CorrectionMode mode) { correctionMode_ = mode; }
    CorrectionMode GetCorrectionMode() const { return correctionMode_; }
    float GetLatency() const { return latencyMs_; }
    std::string GetStatistics() const;

    /**
     * @brief 运行主循环（同步调用）
     */
    void Run();

    /**
     * @brief 获取配置
     */
    const ClientConfig& GetConfig() const { return config_; }

private:
    /**
     * @brief 网络线程
     */
    void NetworkThread();

    /**
     * @brief 模拟线程
     */
    void SimulationThread();

    void ApplySnapshot(const PhysicsWorldSnapshot& snapshot);
    void Predict(float deltaTime);
    void CorrectState(const PhysicsWorldSnapshot& targetSnapshot);
    std::vector<PlayerInput> BuildInputQueue();
    PhysicsObjectState LerpState(const PhysicsObjectState& from,
                                  const PhysicsObjectState& to,
                                  float alpha);

    // 配置
    ClientConfig config_;
    render_callback_t renderCallback_;

    // 网络
    NetworkLayer networkLayer_;
    uint32_t playerId_ = 0;
    std::atomic<bool> networkRunning_ { false };

    // 时间管理
    TimeStepManager timeManager_;

    // 状态
    ClientState state_ = ClientState::DISCONNECTED;
    CorrectionMode correctionMode_ = CorrectionMode::LERP;

    // 快照
    PhysicsWorldSnapshot localSnapshot_;
    PhysicsWorldSnapshot predictedSnapshot_;
    PhysicsWorldSnapshot serverSnapshot_;

    // 输入
    std::vector<PlayerInput> inputQueue_;
    std::mutex inputMutex_;
    uint32_t nextInputTick_ = 0;
    uint32_t lastConfirmedTick_ = 0;

    // 统计
    float latencyMs_ = 0.0f;
    uint32_t packetsReceived_ = 0;
    uint32_t packetsSent_ = 0;
    uint32_t corrections_ = 0;
    uint32_t predictionHits_ = 0;

    std::atomic<bool> running_ = false;
    std::unique_ptr<std::thread> networkThread_;
    std::unique_ptr<std::thread> simulationThread_;
};

} // namespace PhysicsSync
