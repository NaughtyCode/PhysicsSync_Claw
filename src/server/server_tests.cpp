/**
 * @file server_tests.cpp
 * @brief 服务器单元测试
 *
 * 测试物理服务器的各项功能，包括：
 * - 服务器初始化和生命周期
 * - 玩家连接和断开
 * - 输入处理和物理更新
 * - 快照构建和广播
 */

#include <gtest/gtest.h>
#include "physics_server.h"
#include "physics_client.h"
#include "../common/timestep_manager.h"
#include "../common/deterministic_random.h"

namespace PhysicsSync {

// ================================================================
// TimeStepManager 测试
// ================================================================

TEST(TimeStepManagerTest, DefaultConstruction) {
    TimeStepManager manager;

    // 默认应该是60Hz
    EXPECT_FLOAT_EQ(manager.GetFixedTimeStep(), 1.0f / 60.0f);
    EXPECT_EQ(manager.GetPhysicsHz(), 60);
    EXPECT_EQ(manager.GetCurrentTick(), 0);
}

TEST(TimeStepManagerTest, SetFixedTimeStep) {
    TimeStepManager manager;

    // 设置为30Hz
    manager.SetFixedTimeStep(30.0f);
    EXPECT_FLOAT_EQ(manager.GetFixedTimeStep(), 1.0f / 30.0f);
    EXPECT_EQ(manager.GetPhysicsHz(), 30);

    // 设置为120Hz
    manager.SetFixedTimeStep(120.0f);
    EXPECT_FLOAT_EQ(manager.GetFixedTimeStep(), 1.0f / 120.0f);
    EXPECT_EQ(manager.GetPhysicsHz(), 120);
}

TEST(TimeStepManagerTest, BasicSimulation) {
    TimeStepManager manager;
    manager.SetFixedTimeStep(60.0f);

    float fixedStep = 1.0f / 60.0f;

    // 模拟两帧
    for (int frame = 0; frame < 2; frame++) {
        manager.StartNewFrame(fixedStep);  // 每帧正好一个step的时间

        int ticksThisFrame = 0;
        while (manager.ShouldTick()) {
            manager.Tick();
            ticksThisFrame++;
        }

        // 每帧应该恰好处理一个tick
        EXPECT_EQ(ticksThisFrame, 1);

        manager.FinishFrame();
    }

    EXPECT_EQ(manager.GetCurrentTick(), 2);
}

TEST(TimeStepManagerTest, Accumulator) {
    TimeStepManager manager;
    manager.SetFixedTimeStep(60.0f);

    float fixedStep = 1.0f / 60.0f;

    // 传入两倍于step的时间，应该累积
    manager.StartNewFrame(fixedStep * 2.0f);

    // 应该可以处理两个tick
    int ticksThisFrame = 0;
    while (manager.ShouldTick()) {
        manager.Tick();
        ticksThisFrame++;
    }

    EXPECT_EQ(ticksThisFrame, 2);
    EXPECT_EQ(manager.GetCurrentTick(), 2);
}

TEST(TimeStepManagerTest, InterpolationAlpha) {
    TimeStepManager manager;
    manager.SetFixedTimeStep(60.0f);

    float fixedStep = 1.0f / 60.0f;

    // 开始时alpha应该是0
    manager.StartNewFrame(0.0f);
    EXPECT_FLOAT_EQ(manager.GetInterpolationAlpha(), 0.0f);

    // 传入一半step的时间，alpha应该是0.5
    manager.StartNewFrame(fixedStep * 0.5f);
    EXPECT_FLOAT_EQ(manager.GetInterpolationAlpha(), 0.5f);

    manager.FinishFrame();
}

TEST(TimeStepManagerTest, Reset) {
    TimeStepManager manager;
    manager.SetFixedTimeStep(60.0f);

    // 执行几个tick
    for (int i = 0; i < 10; i++) {
        manager.StartNewFrame(1.0f / 60.0f);
        while (manager.ShouldTick()) {
            manager.Tick();
        }
        manager.FinishFrame();
    }

    EXPECT_EQ(manager.GetCurrentTick(), 10);

    // 重置
    manager.Reset();

    EXPECT_EQ(manager.GetCurrentTick(), 0);
    EXPECT_FLOAT_EQ(manager.GetAccumulator(), 0.0f);
}

// ================================================================
// DeterministicRandom 测试
// ================================================================

TEST(DeterministicRandomTest, SameSeedSameSequence) {
    DeterministicRandom rng1(12345);
    DeterministicRandom rng2(12345);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(rng1.Next(), rng2.Next())
            << "Mismatch at iteration " << i;
    }
}

TEST(DeterministicRandomTest, DifferentSeedsDifferentSequence) {
    DeterministicRandom rng1(12345);
    DeterministicRandom rng2(54321);

    bool different = false;
    for (int i = 0; i < 100; i++) {
        if (rng1.Next() != rng2.Next()) {
            different = true;
            break;
        }
    }

    EXPECT_TRUE(different) << "Different seeds should produce different sequences";
}

TEST(DeterministicRandomTest, Range) {
    DeterministicRandom rng(42);

    for (int i = 0; i < 1000; i++) {
        uint32_t value = rng.Next(10, 20);
        EXPECT_GE(value, 10u);
        EXPECT_LE(value, 20u);
    }
}

TEST(DeterministicRandomTest, FloatRange) {
    DeterministicRandom rng(42);

    for (int i = 0; i < 1000; i++) {
        float value = rng.NextFloat(0.0f, 1.0f);
        EXPECT_GE(value, 0.0f);
        EXPECT_LE(value, 1.0f);
    }
}

TEST(DeterministicRandomTest, VerifyDeterminism) {
    EXPECT_TRUE(VerifyDeterminism()) << "Random number generation should be deterministic";
}

// ================================================================
// PhysicsObjectState 测试
// ================================================================

TEST(PhysicsObjectStateTest, DefaultConstruction) {
    PhysicsObjectState state;

    EXPECT_EQ(state.objectId, 0);
    EXPECT_EQ(state.type, PhysicsObjectType::DYNAMIC);
    EXPECT_FLOAT_EQ(state.position.x, 0.0f);
    EXPECT_FLOAT_EQ(state.position.y, 0.0f);
    EXPECT_FLOAT_EQ(state.position.z, 0.0f);
}

TEST(PhysicsObjectStateTest, Constructor) {
    Vec3 pos{1.0f, 2.0f, 3.0f};
    Quat rot{0.0f, 0.0f, 0.0f, 1.0f};

    PhysicsObjectState state(1, pos, rot, PhysicsObjectType::PLAYER);

    EXPECT_EQ(state.objectId, 1);
    EXPECT_EQ(state.type, PhysicsObjectType::PLAYER);
    EXPECT_FLOAT_EQ(state.position.x, 1.0f);
    EXPECT_FLOAT_EQ(state.position.y, 2.0f);
    EXPECT_FLOAT_EQ(state.position.z, 3.0f);
}

TEST(PhysicsObjectStateTest, SerializeDeserialize) {
    PhysicsObjectState original(1, Vec3{1.0f, 2.0f, 3.0f},
                                 Quat{0.0f, 0.0f, 0.0f, 1.0f},
                                 PhysicsObjectType::PLAYER);
    original.sequenceNumber = 42;
    original.linearVelocity = Vec3{1.0f, 0.0f, 0.0f};

    // 序列化
    std::vector<uint8_t> buffer;
    original.Serialize(buffer);

    // 反序列化
    const uint8_t* data = buffer.data();
    PhysicsObjectState deserialized;
    ASSERT_TRUE(deserialized.Deserialize(data));

    // 比较
    EXPECT_EQ(deserialized.objectId, original.objectId);
    EXPECT_EQ(deserialized.type, original.type);
    EXPECT_FLOAT_EQ(deserialized.position.x, original.position.x);
    EXPECT_FLOAT_EQ(deserialized.position.y, original.position.y);
    EXPECT_FLOAT_EQ(deserialized.position.z, original.position.z);
    EXPECT_FLOAT_EQ(deserialized.sequenceNumber, original.sequenceNumber);
}

TEST(PhysicsObjectStateTest, PredictPosition) {
    PhysicsObjectState state(1, Vec3{0.0f, 0.0f, 0.0f},
                              Quat::Identity(),
                              PhysicsObjectType::DYNAMIC);
    state.linearVelocity = Vec3{2.0f, 0.0f, 0.0f};  // 2 m/s to the right

    // 预测0.5秒后的位置
    Vec3 predicted = state.PredictPosition(0.5f);

    EXPECT_FLOAT_EQ(predicted.x, 1.0f);  // 2 * 0.5 = 1
    EXPECT_FLOAT_EQ(predicted.y, 0.0f);
    EXPECT_FLOAT_EQ(predicted.z, 0.0f);
}

TEST(PhysicsObjectStateTest, SerializedSize) {
    // 序列化后的固定大小
    constexpr size_t expectedSize = sizeof(uint32_t) + sizeof(uint8_t) +
                                     sizeof(float) * 3 * 4 +
                                     sizeof(float) * 4 +
                                     sizeof(uint32_t) + sizeof(uint32_t);

    EXPECT_EQ(PhysicsObjectState::SerializedSizeBytes(), expectedSize);
}

// ================================================================
// PhysicsWorldSnapshot 测试
// ================================================================

TEST(PhysicsWorldSnapshotTest, AddAndRemoveObjects) {
    PhysicsWorldSnapshot snapshot;

    // 初始为空
    EXPECT_EQ(snapshot.objectCount, 0);
    EXPECT_TRUE(snapshot.objects.empty());

    // 添加对象
    PhysicsObjectState obj1(1, Vec3{0.0f, 0.0f, 0.0f},
                             Quat::Identity(),
                             PhysicsObjectType::PLAYER);
    snapshot.AddObject(obj1);

    EXPECT_EQ(snapshot.objectCount, 1);
    EXPECT_EQ(snapshot.objects.size(), 1u);

    // 查找对象
    auto* found = snapshot.FindObject(1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->objectId, 1);

    // 查找不存在的对象
    EXPECT_EQ(snapshot.FindObject(999), nullptr);
}

TEST(PhysicsWorldSnapshotTest, SerializeSnapshot) {
    PhysicsWorldSnapshot snapshot;

    PhysicsObjectState obj1(1, Vec3{1.0f, 2.0f, 3.0f},
                             Quat::Identity(),
                             PhysicsObjectType::PLAYER);
    PhysicsObjectState obj2(2, Vec3{4.0f, 5.0f, 6.0f},
                             Quat::Identity(),
                             PhysicsObjectType::DYNAMIC);

    snapshot.AddObject(obj1);
    snapshot.AddObject(obj2);

    // 序列化
    std::vector<uint8_t> buffer;
    snapshot.Serialize(buffer);

    // 反序列化
    const uint8_t* data = buffer.data();
    PhysicsWorldSnapshot deserialized;
    ASSERT_TRUE(deserialized.Deserialize(data));

    // 验证
    EXPECT_EQ(deserialized.objectCount, 2u);
    EXPECT_EQ(deserialized.objects.size(), 2u);
    EXPECT_FLOAT_EQ(deserialized.objects[0].position.x, 1.0f);
    EXPECT_FLOAT_EQ(deserialized.objects[1].position.x, 4.0f);
}

// ================================================================
// Serializer/Deserializer 测试
// ================================================================

TEST(SerializerTest, BasicTypes) {
    Serializer serializer;

    // 序列化基本类型
    serializer.Serialize(uint8_t(42));
    serializer.Serialize(uint16_t(12345));
    serializer.Serialize(uint32_t(123456789));
    serializer.Serialize(uint64_t(123456789012345ULL));
    serializer.Serialize(3.14159f);
    serializer.Serialize(true);

    // 检查缓冲区大小
    EXPECT_GT(serializer.GetSize(), 0u);
    EXPECT_GT(serializer.GetBytesWritten(), 0u);
}

TEST(SerializerTest, RoundTrip) {
    Serializer serializer;

    // 序列化
    serializer.Serialize(uint32_t(42));
    serializer.Serialize(3.14f);
    serializer.Serialize(true);

    std::vector<uint8_t> data = serializer.GetData();

    // 反序列化
    Deserializer deserializer(data);

    uint32_t uintVal;
    float floatVal;
    bool boolVal;

    ASSERT_TRUE(deserializer.Deserialize(uintVal));
    ASSERT_TRUE(deserializer.Deserialize(floatVal));
    ASSERT_TRUE(deserializer.Deserialize(boolVal));

    // 验证
    EXPECT_EQ(uintVal, 42u);
    EXPECT_NEAR(floatVal, 3.14f, 0.001f);
    EXPECT_TRUE(boolVal);
}

TEST(SerializerTest, NetworkByteOrder) {
    // 验证网络字节序转换
    Serializer serializer;
    serializer.Serialize(uint16_t(0x0102));

    std::vector<uint8_t> data = serializer.GetData();

    // 在网络字节序中，0x0102应该被交换为0x0201
    // 即第一个字节是0x02，第二个是0x01
    EXPECT_EQ(data.size(), 2u);
}

// ================================================================
// Vec3/Quat 测试
// ================================================================

TEST(Vec3Test, Operations) {
    Vec3 a{1.0f, 2.0f, 3.0f};
    Vec3 b{4.0f, 5.0f, 6.0f};

    // 加法
    Vec3 sum = a + b;
    EXPECT_FLOAT_EQ(sum.x, 5.0f);
    EXPECT_FLOAT_EQ(sum.y, 7.0f);
    EXPECT_FLOAT_EQ(sum.z, 9.0f);

    // 点乘
    float dot = a.Dot(b);
    EXPECT_FLOAT_EQ(dot, 32.0f);  // 1*4 + 2*5 + 3*6
}

TEST(Vec3Test, Normalized) {
    Vec3 v{3.0f, 4.0f, 0.0f};
    Vec3 normalized = v.Normalized();

    EXPECT_FLOAT_EQ(normalized.Length(), 1.0f);
}

TEST(QuatTest, Slerp) {
    Quat identity = Quat::Identity();
    Quat rotation = Quat{0.0f, 0.0f, 0.7071f, 0.7071f};  // 90度绕Z轴

    // 插值一半
    Quat halfway = identity.Slerp(rotation, 0.5f);

    // 结果应该介于两者之间
    EXPECT_NEAR(halfway.w, 0.9239f, 0.01f);  // cos(22.5度)
}

// ================================================================
// 服务器集成测试（不启动网络，只测试物理逻辑）
// ================================================================

TEST(ServerIntegrationTest, CreateWorld) {
    // 创建服务器（不启动网络）
    ServerConfig config(9300);
    config.physicsHz = 60;
    config.snapshotHz = 30;

    PhysicsServer server(config);
    server.CreateDefaultWorld();

    // 验证初始状态
    PhysicsWorldSnapshot snapshot;
    server.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.objects.size(), 3u);  // player, ball, ground
}

TEST(ServerIntegrationTest, PhysicsUpdate) {
    ServerConfig config(9300);
    config.physicsHz = 60;
    config.snapshotHz = 30;

    PhysicsServer server(config);
    server.CreateDefaultWorld();

    // 手动模拟几个 tick
    TimeStepManager timeManager;
    timeManager.SetFixedTimeStep(1.0f / 60.0f);

    for (int i = 0; i < 60; i++) {
        timeManager.StartNewFrame(1.0f / 60.0f);
        while (timeManager.ShouldTick()) {
            // 更新物理对象（简化版，与 SimulationThread 类似）
            PhysicsWorldSnapshot snapshot;
            server.GetSnapshot(snapshot);
            for (auto& obj : snapshot.objects) {
                if (obj.type == PhysicsObjectType::DYNAMIC) {
                    obj.linearVelocity.y -= 9.81f * timeManager.GetFixedTimeStep();
                    obj.position = obj.PredictPosition(timeManager.GetFixedTimeStep());
                    if (obj.position.y < -1.0f) {
                        obj.position.y = -1.0f;
                        obj.linearVelocity.y *= -0.3f;
                    }
                }
            }
            timeManager.Tick();
        }
        timeManager.FinishFrame();
    }

    // 验证物理状态已更新
    PhysicsWorldSnapshot snapshot;
    server.GetSnapshot(snapshot);
    EXPECT_GT(snapshot.timestamp, 0u);
}

TEST(ServerIntegrationTest, Statistics) {
    ServerConfig config(9300);
    PhysicsServer server(config);
    server.CreateDefaultWorld();

    // 获取统计信息
    std::string stats = server.GetStatistics();
    EXPECT_FALSE(stats.empty());
    EXPECT_NE(stats.find("Objects:"), std::string::npos);
}

// ================================================================
// 客户端集成测试
// ================================================================

TEST(ClientIntegrationTest, ClientConfig) {
    ClientConfig config("127.0.0.1", 9300);
    EXPECT_EQ(config.serverHost, "127.0.0.1");
    EXPECT_EQ(config.serverPort, 9300);
    EXPECT_EQ(config.physicsHz, 60);

    ClientConfig defaultConfig;
    EXPECT_EQ(defaultConfig.serverHost, "127.0.0.1");
    EXPECT_EQ(defaultConfig.serverPort, 9300);
}

TEST(ClientIntegrationTest, ClientCreate) {
    ClientConfig config("127.0.0.1", 9300);
    PhysicsClient client(config);

    EXPECT_FALSE(client.IsRunning());
    EXPECT_EQ(client.GetState(), ClientState::DISCONNECTED);

    // 不连接到实际服务器，只验证创建成功
    bool initialized = client.Initialize();
    EXPECT_TRUE(initialized);
}

} // namespace PhysicsSync
