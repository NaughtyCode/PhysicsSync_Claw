/**
 * @file physics_client.cpp
 * @brief 物理客户端实现 - 集成 NetworkLayer 网络层
 *
 * 实现客户端预测与校正逻辑：
 * 1. 接收服务器快照并应用到本地状态
 * 2. 基于输入序列进行客户端预测
 * 3. 当收到权威状态时进行校正
 * 4. 提供渲染所需的状态（带插值）
 * 5. 通过网络层 (NetworkLayer) 收发数据
 */

#include "physics_client.h"
#include "../common/network_protocol.h"
#include "../common/serializer.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace PhysicsSync {

// ================================================================
// PhysicsClient 实现
// ================================================================

PhysicsClient::PhysicsClient(const ClientConfig& config,
                              render_callback_t renderCallback)
    : config_(config)
    , renderCallback_(renderCallback)
    , state_(ClientState::DISCONNECTED)
    , correctionMode_(config.defaultCorrection)
{
    timeManager_.SetFixedTimeStep(1.0f / config_.physicsHz);
}

PhysicsClient::~PhysicsClient() {
    Stop();
}

bool PhysicsClient::Initialize() {
    std::cout << "[PhysicsClient] Initializing..." << std::endl;
    std::cout << "  Server: " << config_.serverHost << ":"
              << config_.serverPort << std::endl;
    std::cout << "  Physics Hz: " << config_.physicsHz << std::endl;
    std::cout << "  Correction: "
              << (correctionMode_ == CorrectionMode::LERP ? "LERP" :
                  correctionMode_ == CorrectionMode::SNAP ? "SNAP" : "ROLLBACK")
              << std::endl;

    // 创建初始本地状态
    localSnapshot_.Clear();

    std::cout << "[PhysicsClient] Initialization complete." << std::endl;
    return true;
}

bool PhysicsClient::Connect() {
    if (state_ == ClientState::RUNNING || state_ == ClientState::CONNECTED) {
        return true;
    }

    std::cout << "[PhysicsClient] Connecting to " << config_.serverHost
              << ":" << config_.serverPort << "..." << std::endl;

    state_ = ClientState::CONNECTING;

    // 设置网络层为客户端模式
    if (!networkLayer_.CreateAsClient(config_.serverHost.c_str(), config_.serverPort)) {
        std::cerr << "[PhysicsClient] Failed to create client socket." << std::endl;
        state_ = ClientState::DISCONNECTED;
        return false;
    }

    // 发送连接请求
    if (!networkLayer_.Connect()) {
        std::cerr << "[PhysicsClient] Failed to send connect request." << std::endl;
        state_ = ClientState::DISCONNECTED;
        return false;
    }

    std::cout << "[PhysicsClient] Connection initiated." << std::endl;
    return true;
}

void PhysicsClient::Disconnect() {
    std::cout << "[PhysicsClient] Disconnecting..." << std::endl;

    running_.store(false);
    networkRunning_.store(false);

    if (simulationThread_ && simulationThread_->joinable()) {
        simulationThread_->join();
    }
    if (networkThread_ && networkThread_->joinable()) {
        networkThread_->join();
    }

    networkLayer_.Close();
    state_ = ClientState::DISCONNECTED;

    std::cout << "[PhysicsClient] Disconnected." << std::endl;
}

void PhysicsClient::Start() {
    if (running_.load()) {
        std::cout << "[PhysicsClient] Already running." << std::endl;
        return;
    }

    std::cout << "[PhysicsClient] Starting client..." << std::endl;
    running_.store(true);
    networkRunning_.store(true);

    // 确保已连接
    if (state_ == ClientState::DISCONNECTED || state_ == ClientState::CONNECTING) {
        if (!Connect()) {
            return;
        }
    }

    simulationThread_ = std::make_unique<std::thread>(
        &PhysicsClient::SimulationThread, this);
    networkThread_ = std::make_unique<std::thread>(
        &PhysicsClient::NetworkThread, this);
}

void PhysicsClient::Stop() {
    running_.store(false);
    networkRunning_.store(false);

    if (simulationThread_ && simulationThread_->joinable()) {
        simulationThread_->join();
    }
    if (networkThread_ && networkThread_->joinable()) {
        networkThread_->join();
    }

    networkLayer_.Close();
    state_ = ClientState::DISCONNECTED;
}

bool PhysicsClient::SendInput(const PlayerInput& input) {
    if (!IsConnected()) return false;

    std::lock_guard<std::mutex> lock(inputMutex_);
    inputQueue_.push_back(input);
    packetsSent_++;

    // 立即发送到网络层
    auto msg = std::make_unique<PlayerInputMessage>();
    msg->playerId = playerId_;
    msg->tick = nextInputTick_;
    input.Serialize(msg->inputData);

    return networkLayer_.Send(std::move(msg));
}

bool PhysicsClient::ProcessSnapshot(const PhysicsWorldSnapshot& snapshot) {
    if (snapshot.objects.empty()) return false;

    serverSnapshot_ = snapshot;
    ApplySnapshot(snapshot);
    lastConfirmedTick_ = snapshot.snapshotId;
    packetsReceived_++;

    std::cout << "[PhysicsClient] Received snapshot " << snapshot.snapshotId
              << " with " << snapshot.objects.size() << " objects." << std::endl;

    if (state_ == ClientState::SYNCING) {
        state_ = ClientState::RUNNING;
    }

    return true;
}

bool PhysicsClient::GetWorldState(PhysicsWorldSnapshot& output) {
    output = predictedSnapshot_;
    return true;
}

std::string PhysicsClient::GetStatistics() const {
    std::ostringstream oss;
    oss << "=== PhysicsClient Statistics ===" << std::endl;
    oss << "  State: ";
    switch (state_) {
        case ClientState::DISCONNECTED: oss << "DISCONNECTED"; break;
        case ClientState::CONNECTING:   oss << "CONNECTING"; break;
        case ClientState::CONNECTED:    oss << "CONNECTED"; break;
        case ClientState::SYNCING:      oss << "SYNCING"; break;
        case ClientState::RUNNING:      oss << "RUNNING"; break;
        case ClientState::RECONNECTING: oss << "RECONNECTING"; break;
        default:                        oss << "UNKNOWN"; break;
    }
    oss << std::endl;
    oss << "  Latency: " << latencyMs_ << "ms" << std::endl;
    oss << "  Network: " << networkLayer_.GetStats() << std::endl;
    oss << "  Packets Received: " << packetsReceived_ << std::endl;
    oss << "  Packets Sent: " << packetsSent_ << std::endl;
    oss << "  Corrections: " << corrections_ << std::endl;
    oss << "  Prediction Hits: " << predictionHits_ << std::endl;
    oss << "  Correction Mode: "
        << (correctionMode_ == CorrectionMode::LERP ? "LERP" :
            correctionMode_ == CorrectionMode::SNAP ? "SNAP" : "ROLLBACK")
        << std::endl;
    return oss.str();
}

void PhysicsClient::Run() {
    std::cout << "[PhysicsClient] Starting main loop..." << std::endl;

    // 确保已连接
    if (!IsConnected()) {
        if (!Connect()) {
            std::cerr << "[PhysicsClient] Failed to connect, exiting." << std::endl;
            return;
        }
        // 连接成功后启动线程
        Start();
    }

    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    while (running_.load()) {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaTime = now - lastFrameTime;
        lastFrameTime = now;

        float dt = std::min(deltaTime.count(), 0.1f);

        // 开始新帧
        timeManager_.StartNewFrame(dt);

        // 处理物理 tick
        while (timeManager_.ShouldTick()) {
            Predict(dt);
            timeManager_.Tick();
        }

        // 更新预测状态
        predictedSnapshot_ = localSnapshot_;

        // 结束帧
        timeManager_.FinishFrame();

        // 调用渲染回调
        if (renderCallback_) {
            renderCallback_(
                static_cast<uint32_t>(timeManager_.GetCurrentTick()),
                timeManager_.GetInterpolationAlpha());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "[PhysicsClient] Main loop stopped." << std::endl;
}

// ================================================================
// 私有方法实现
// ================================================================

void PhysicsClient::NetworkThread() {
    std::cout << "[PhysicsClient] Network thread started." << std::endl;

    bool waitingForConnectAck = true;
    uint32_t pingInterval = 0;

    while (running_.load() && networkRunning_.load()) {
        // 更新网络层
        networkLayer_.Update();

        // 处理所有接收到的消息
        while (auto msg = networkLayer_.Receive()) {
            uint16_t type = msg->GetType();

            switch (static_cast<ServerMessageType>(type)) {
                case ServerMessageType::CONNECT_ACK: {
                    auto* ack = dynamic_cast<ConnectAckMessage*>(msg.get());
                    if (ack) {
                        playerId_ = ack->playerId;
                        latencyMs_ = ack->latency;
                        waitingForConnectAck = false;
                        state_ = ClientState::CONNECTED;
                        std::cout << "[PhysicsClient] Connected! Player ID: "
                                  << playerId_ << ", Server tick: "
                                  << ack->serverTick << std::endl;

                        // 进入同步模式，请求完整快照
                        state_ = ClientState::SYNCING;
                    }
                    break;
                }

                case ServerMessageType::WORLD_SNAPSHOT: {
                    auto* snapMsg = dynamic_cast<WorldSnapshotMessage*>(msg.get());
                    if (snapMsg && !snapMsg->stateData.empty()) {
                        const uint8_t* pData = snapMsg->stateData.data();
                        PhysicsWorldSnapshot snap;
                        if (snap.Deserialize(pData)) {
                            ProcessSnapshot(snap);
                        }
                    }
                    break;
                }

                case ServerMessageType::PING: {
                    // 回复 PONG
                    auto pong = std::make_unique<PingMessage>();
                    pong->timestamp = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count());
                    pong->nonce = 0;
                    networkLayer_.Send(std::move(pong));
                    break;
                }

                case ServerMessageType::PONG: {
                    auto* pong = dynamic_cast<PingMessage*>(msg.get());
                    if (pong && pong->timestamp > 0) {
                        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
                        float rtt = static_cast<float>(now - pong->timestamp);
                        if (rtt > 0 && latencyMs_ == 0) {
                            latencyMs_ = rtt;
                        } else if (latencyMs_ > 0) {
                            latencyMs_ = latencyMs_ * 0.75f + rtt * 0.25f;
                        }
                    }
                    break;
                }

                case ServerMessageType::ERROR_MSG: {
                    auto* err = dynamic_cast<ErrorMessage*>(msg.get());
                    if (err) {
                        std::cerr << "[PhysicsClient] Server error: "
                                  << err->message << " (code: " << err->errorCode << ")"
                                  << std::endl;
                    }
                    break;
                }

                default: {
                    std::cout << "[PhysicsClient] Unknown server message: " << type << std::endl;
                    break;
                }
            }
        }

        // 定期发送 ping
        pingInterval++;
        if (pingInterval >= 10) {  // ~10 * 16ms = 160ms ping interval
            auto ping = std::make_unique<PingMessage>();
            ping->timestamp = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            networkLayer_.Send(std::move(ping));
            pingInterval = 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "[PhysicsClient] Network thread stopped." << std::endl;
}

void PhysicsClient::SimulationThread() {
    std::cout << "[PhysicsClient] Simulation thread started." << std::endl;

    auto lastFrameTime = std::chrono::high_resolution_clock::now();

    while (running_.load()) {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaTime = now - lastFrameTime;
        lastFrameTime = now;

        float dt = std::min(deltaTime.count(), 0.1f);

        timeManager_.StartNewFrame(dt);

        // 处理物理 tick
        while (timeManager_.ShouldTick()) {
            // 处理待发送的输入
            {
                std::lock_guard<std::mutex> lock(inputMutex_);
                for (auto& input : inputQueue_) {
                    input.inputTick = nextInputTick_;
                    input.ComputeHash();
                    nextInputTick_++;

                    // 发送输入到服务器
                    if (IsConnected()) {
                        auto msg = std::make_unique<PlayerInputMessage>();
                        msg->playerId = playerId_;
                        msg->tick = input.inputTick;
                        input.Serialize(msg->inputData);
                        networkLayer_.Send(std::move(msg));
                    }
                }
                inputQueue_.clear();
            }

            // 执行客户端预测
            Predict(dt);

            timeManager_.Tick();
        }

        // 更新预测状态
        predictedSnapshot_ = localSnapshot_;

        timeManager_.FinishFrame();

        // 调用渲染回调
        if (renderCallback_) {
            renderCallback_(
                static_cast<uint32_t>(timeManager_.GetCurrentTick()),
                timeManager_.GetInterpolationAlpha());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "[PhysicsClient] Simulation thread stopped." << std::endl;
}

void PhysicsClient::ApplySnapshot(const PhysicsWorldSnapshot& snapshot) {
    // 将服务器快照应用到本地状态
    if (localSnapshot_.objects.empty()) {
        // 首次同步，直接应用
        localSnapshot_.Clear();
        for (const auto& obj : snapshot.objects) {
            localSnapshot_.AddObject(obj);
        }
        std::cout << "[PhysicsClient] Initial sync applied." << std::endl;
        return;
    }

    for (const auto& serverObj : snapshot.objects) {
        auto* localObj = localSnapshot_.FindObject(serverObj.objectId);

        if (localObj) {
            // 对象已存在，应用校正
            if (correctionMode_ == CorrectionMode::LERP) {
                *localObj = LerpState(*localObj, serverObj, 0.5f);
            } else {
                *localObj = serverObj;
            }
            corrections_++;
        } else {
            // 新对象，直接添加
            localSnapshot_.AddObject(serverObj);
        }
    }
}

void PhysicsClient::Predict(float deltaTime) {
    // 基于最后确认的状态进行预测
    for (auto& obj : localSnapshot_.objects) {
        obj.position = obj.PredictPosition(deltaTime);
        obj.rotation = obj.PredictRotation(deltaTime);
    }
    predictionHits_++;
}

void PhysicsClient::CorrectState(const PhysicsWorldSnapshot& targetSnapshot) {
    for (auto& localObj : localSnapshot_.objects) {
        auto* serverObj = targetSnapshot.FindObject(localObj.objectId);
        if (serverObj) {
            localObj.position = serverObj->position;
            localObj.rotation = serverObj->rotation;
            localObj.linearVelocity = serverObj->linearVelocity;
            localObj.angularVelocity = serverObj->angularVelocity;
        }
    }
}

std::vector<PlayerInput> PhysicsClient::BuildInputQueue() {
    std::lock_guard<std::mutex> lock(inputMutex_);
    std::vector<PlayerInput> toSend;
    toSend.swap(inputQueue_);
    return toSend;
}

PhysicsObjectState PhysicsClient::LerpState(const PhysicsObjectState& from,
                                             const PhysicsObjectState& to,
                                             float alpha) {
    PhysicsObjectState result = from;

    result.position = from.position + (to.position - from.position) * alpha;
    result.rotation = from.rotation.Slerp(to.rotation, alpha);
    result.linearVelocity = from.linearVelocity +
                            (to.linearVelocity - from.linearVelocity) * alpha;
    result.angularVelocity = from.angularVelocity +
                             (to.angularVelocity - from.angularVelocity) * alpha;

    return result;
}

} // namespace PhysicsSync
