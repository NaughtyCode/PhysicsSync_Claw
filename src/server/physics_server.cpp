/**
 * @file physics_server.cpp
 * @brief 物理服务器实现 - 集成 NetworkLayer 网络层
 *
 * 实现服务器权威物理模拟，包括：
 * 1. 固定时间步长模拟循环
 * 2. 输入处理和物理更新
 * 3. 快照构建和广播
 * 4. 玩家连接管理 (基于 NetworkLayer)
 * 5. 网络接收线程
 */

#include "physics_server.h"
#include "../common/network_protocol.h"
#include "../common/serializer.h"
#include <thread>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace PhysicsSync {

// ================================================================
// PhysicsServer 实现
// ================================================================

PhysicsServer::PhysicsServer(const ServerConfig& config)
    : config_(config)
    , nextPlayerId_(1)
    , lastBroadcastTick_(0)
    , deterministicRng_(42)  // 固定种子确保确定性
{
    timeManager_.SetFixedTimeStep(1.0f / config_.physicsHz);
}

PhysicsServer::~PhysicsServer() {
    Stop();
}

bool PhysicsServer::Initialize() {
    std::cout << "[PhysicsServer] Initializing..." << std::endl;
    std::cout << "  Port: " << config_.listenPort << std::endl;
    std::cout << "  Physics Hz: " << config_.physicsHz << std::endl;
    std::cout << "  Snapshot Hz: " << config_.snapshotHz << std::endl;
    std::cout << "  Max Clients: " << config_.maxClients << std::endl;

    // 创建默认物理世界（测试用）
    CreateDefaultWorld();

    // 设置网络层为服务器模式
    if (!networkLayer_.CreateAsServer("0.0.0.0", config_.listenPort)) {
        std::cerr << "[PhysicsServer] Failed to create server socket on port "
                  << config_.listenPort << std::endl;
        return false;
    }

    std::cout << "[PhysicsServer] Server socket listening on port "
              << config_.listenPort << std::endl;

    std::cout << "[PhysicsServer] Initialization complete." << std::endl;
    return true;
}

void PhysicsServer::Start() {
    if (running_.load()) {
        std::cout << "[PhysicsServer] Already running." << std::endl;
        return;
    }

    std::cout << "[PhysicsServer] Starting server threads..." << std::endl;
    running_.store(true);
    networkRunning_.store(true);

    simulationThread_ = std::make_unique<std::thread>(
        &PhysicsServer::SimulationThread, this);
    networkThread_ = std::make_unique<std::thread>(
        &PhysicsServer::NetworkThread, this);
}

void PhysicsServer::Stop() {
    if (!running_.load()) {
        return;
    }

    std::cout << "[PhysicsServer] Stopping..." << std::endl;
    running_.store(false);
    networkRunning_.store(false);

    if (simulationThread_ && simulationThread_->joinable()) {
        simulationThread_->join();
    }
    if (networkThread_ && networkThread_->joinable()) {
        networkThread_->join();
    }

    // 关闭所有玩家连接
    {
        std::lock_guard<std::mutex> lock(playersMutex_);
        for (auto& [id, player] : players_) {
            player->Close();
        }
        players_.clear();
    }

    networkLayer_.Close();

    std::cout << "[PhysicsServer] Stopped." << std::endl;
}

uint32_t PhysicsServer::AddPlayer(const NetEndpoint& endpoint) {
    std::lock_guard<std::mutex> lock(playersMutex_);

    uint32_t playerId = nextPlayerId_++;

    auto player = std::make_unique<ServerPlayer>(playerId, endpoint.port);
    player->SetEndpoint(endpoint);
    players_[playerId] = std::move(player);

    std::cout << "[PhysicsServer] Player " << playerId
              << " connected from " << endpoint.ToString()
              << ". Total players: " << players_.size() << std::endl;

    return playerId;
}

void PhysicsServer::RemovePlayer(uint32_t playerId) {
    std::lock_guard<std::mutex> lock(playersMutex_);
    auto it = players_.find(playerId);
    if (it != players_.end()) {
        std::cout << "[PhysicsServer] Player " << playerId
                  << " disconnected. Total players: "
                  << (players_.size() - 1) << std::endl;
        players_.erase(it);
    }
}

uint32_t PhysicsServer::GetPlayerCount() const {
    std::lock_guard<std::mutex> lock(playersMutex_);
    return static_cast<uint32_t>(players_.size());
}

std::string PhysicsServer::GetStatistics() const {
    std::ostringstream oss;
    oss << "=== PhysicsServer Statistics ===" << std::endl;
    oss << timeManager_.GetStatistics();
    oss << "Players: " << GetPlayerCount() << std::endl;
    oss << "Current Tick: " << GetCurrentTick() << std::endl;
    oss << "NetworkLayer: " << networkLayer_.GetStats() << std::endl;

    std::lock_guard<std::mutex> lock(snapshotMutex_);
    oss << "Snapshot Objects: " << currentSnapshot_.objects.size() << std::endl;
    oss << "Last Broadcast Tick: " << lastBroadcastTick_ << std::endl;

    return oss.str();
}

void PhysicsServer::HandlePlayerInput(const uint8_t* data, size_t len) {
    if (len < sizeof(PlayerInput)) return;

    const uint8_t* pData = data;
    PlayerInput input;
    if (!input.Deserialize(pData)) return;

    // 校验输入哈希
    input.ComputeHash();

    std::lock_guard<std::mutex> lock(inputMutex_);
    pendingInputs_.emplace_back(input, 0);  // playerId set during processing

    std::cout << "[PhysicsServer] Received input from player "
              << input.playerId << " tick=" << input.inputTick
              << " move=(" << input.moveX << "," << input.moveY << ")"
              << std::endl;
}

void PhysicsServer::BroadcastSnapshot() {
    if (GetCurrentTick() - lastBroadcastTick_ < config_.physicsHz / config_.snapshotHz) {
        return;
    }
    BroadcastSnapshotToAll();
    lastBroadcastTick_ = GetCurrentTick();
}

// ================================================================
// 私有线程实现
// ================================================================

void PhysicsServer::NetworkThread() {
    std::cout << "[PhysicsServer] Network thread started." << std::endl;

    uint32_t playerIdCounter = 1;

    while (running_.load() && networkRunning_.load()) {
        // 更新网络层
        networkLayer_.Update();

        // 处理所有接收到的消息
        while (auto msg = networkLayer_.Receive()) {
            uint16_t type = msg->GetType();

            // 直接处理序列化数据
            // 先构建帧
            std::vector<uint8_t> dummy;
            msg->Serialize(dummy);

            switch (static_cast<ClientMessageType>(type)) {
                case ClientMessageType::CONNECT_REQUEST: {
                    // 新连接
                    std::cout << "[PhysicsServer] Connect request received." << std::endl;

                    // 分配玩家ID并添加连接
                    std::lock_guard<std::mutex> lock(playersMutex_);
                    uint32_t pid = playerIdCounter++;
                    auto player = std::make_unique<ServerPlayer>(pid, 0);
                    players_[pid] = std::move(player);
                    playerIdCounter = pid + 1;

                    // 发送连接确认
                    auto ack = std::make_unique<ConnectAckMessage>();
                    ack->playerId = pid;
                    ack->serverTick = GetCurrentTick();
                    ack->latency = networkLayer_.GetLatency();
                    networkLayer_.Send(std::move(ack));

                    std::cout << "[PhysicsServer] Player " << pid
                              << " assigned ID, total: " << players_.size() << std::endl;
                    break;
                }

                case ClientMessageType::PLAYER_INPUT: {
                    // 需要先收到 ConnectAck 才能知道玩家ID
                    // 这里使用原始数据中的 playerId
                    if (dummy.size() >= sizeof(PlayerInput)) {
                        const uint8_t* pData = dummy.data();
                        PlayerInput input;
                        if (input.Deserialize(pData)) {
                            input.ComputeHash();

                            std::lock_guard<std::mutex> lock(inputMutex_);
                            pendingInputs_.emplace_back(input, 0);

                            std::cout << "[PhysicsServer] Input from player "
                                      << input.playerId
                                      << " tick=" << input.inputTick << std::endl;
                        }
                    }
                    break;
                }

                case ClientMessageType::PING_REQUEST: {
                    // 回复 PONG
                    auto pong = std::make_unique<PingMessage>();
                    pong->timestamp = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count());
                    pong->nonce = 0;
                    networkLayer_.Send(std::move(pong));
                    break;
                }

                default: {
                    std::cout << "[PhysicsServer] Unknown message type: " << type << std::endl;
                    break;
                }
            }
        }

        // 小休眠，防止 CPU 占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "[PhysicsServer] Network thread stopped." << std::endl;
}

void PhysicsServer::SimulationThread() {
    std::cout << "[PhysicsServer] Simulation thread started." << std::endl;

    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    while (running_.load()) {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaTime = now - lastFrameTime;
        lastFrameTime = now;

        // 防止 deltaTime 过大
        float dt = std::min(deltaTime.count(), 0.1f);

        // 开始新帧
        timeManager_.StartNewFrame(dt);

        // 处理所有物理 tick
        while (timeManager_.ShouldTick()) {
            // 处理待处理的输入
            ProcessPendingInputs();

            // 更新物理世界
            currentSnapshot_.timestamp = GetCurrentTick();

            // 模拟物理更新（简化实现）
            for (auto& obj : currentSnapshot_.objects) {
                if (obj.type == PhysicsObjectType::DYNAMIC) {
                    // 简单重力 + 位置更新
                    obj.linearVelocity.y -= 9.81f * timeManager_.GetFixedTimeStep();
                    obj.position = obj.PredictPosition(timeManager_.GetFixedTimeStep());

                    // 简单地面碰撞
                    if (obj.position.y < -1.0f) {
                        obj.position.y = -1.0f;
                        obj.linearVelocity.y *= -0.3f;
                        if (std::abs(obj.linearVelocity.y) < 0.1f) {
                            obj.linearVelocity.y = 0.0f;
                        }
                    }
                }
            }

            // 执行 tick
            timeManager_.Tick();
        }

        // 定期广播快照
        BroadcastSnapshot();

        // 结束帧
        timeManager_.FinishFrame();

        // 小休眠
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "[PhysicsServer] Simulation thread stopped." << std::endl;
}

void PhysicsServer::ProcessPendingInputs() {
    std::lock_guard<std::mutex> lock(inputMutex_);

    for (auto& [input, /*playerId*/] : pendingInputs_) {
        // 根据输入更新刚体速度
        auto* playerObj = currentSnapshot_.FindObject(input.playerId);
        if (playerObj && (input.moveX != 0.0f || input.moveY != 0.0f)) {
            playerObj->linearVelocity.x = input.moveX * 5.0f;
            playerObj->linearVelocity.y = input.moveY * 5.0f;
        }
    }

    pendingInputs_.clear();
}

void PhysicsServer::BroadcastSnapshotToAll() {
    std::lock_guard<std::mutex> lock(snapshotMutex_);

    currentSnapshot_.snapshotId = GetCurrentTick();
    currentSnapshot_.objectCount = static_cast<uint32_t>(
        currentSnapshot_.objects.size());

    // 序列化快照
    std::vector<uint8_t> snapshotData;
    currentSnapshot_.Serialize(snapshotData);

    // 构建 WorldSnapshotMessage
    auto msg = std::make_unique<WorldSnapshotMessage>();
    msg->snapshotId = GetCurrentTick();
    msg->tick = GetCurrentTick();
    msg->stateData = std::move(snapshotData);

    // 发送给所有玩家
    if (players_.empty()) {
        // 如果没有玩家，广播到网络层
        networkLayer_.Send(std::move(msg));
        return;
    }

    // 服务器模式下 NetworkLayer 自动路由到已知端点
    networkLayer_.Send(std::move(msg));

    std::cout << "[PhysicsServer] Broadcast snapshot " << GetCurrentTick()
              << " with " << currentSnapshot_.objects.size()
              << " objects to " << players_.size() << " player(s)." << std::endl;
}

void PhysicsServer::CreateDefaultWorld() {
    currentSnapshot_.Clear();

    // 添加玩家角色
    PhysicsObjectState playerState(
        1, Vec3{0.0f, 2.0f, 0.0f}, Quat::Identity(),
        PhysicsObjectType::PLAYER);
    currentSnapshot_.AddObject(playerState);

    // 添加动态物体（球）
    PhysicsObjectState ballState(
        2, Vec3{5.0f, 5.0f, 0.0f}, Quat::Identity(),
        PhysicsObjectType::DYNAMIC);
    ballState.linearVelocity = Vec3{1.0f, 0.0f, 0.0f};
    currentSnapshot_.AddObject(ballState);

    // 添加静态地面
    PhysicsObjectState groundState(
        3, Vec3{0.0f, -1.0f, 0.0f}, Quat::Identity(),
        PhysicsObjectType::STATIC);
    currentSnapshot_.AddObject(groundState);

    lastBroadcastTick_ = currentSnapshot_.snapshotId;
    std::cout << "[PhysicsServer] Created default world with "
              << currentSnapshot_.objects.size() << " objects." << std::endl;
}

} // namespace PhysicsSync
