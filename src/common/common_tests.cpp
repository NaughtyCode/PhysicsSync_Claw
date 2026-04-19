/**
 * @file common_tests.cpp
 * @brief 公共库单元测试
 * 
 * 测试物理状态、序列化器、时间步长管理器、确定性随机数等核心模块。
 */

#include <gtest/gtest.h>
#include <cstring>
#include <array>

#include "physics_state.h"
#include "serializer.h"
#include "timestep_manager.h"
#include "deterministic_random.h"
#include "network_protocol.h"

// ======== ================== ==== ========= ============================
// Vec3 测试
// ======== ================== ==== ========= ============================

TEST(Vec3Test, Constructor) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST(Vec3Test, DefaultConstructor) {
    Vec3 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vec3Test, Addition) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    Vec3 c = a + b;
    EXPECT_FLOAT_EQ(c.x, 5.0f);
    EXPECT_FLOAT_EQ(c.y, 7.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);
}

TEST(Vec3Test, Subtraction) {
    Vec3 a(4.0f, 5.0f, 6.0f);
    Vec3 b(1.0f, 2.0f, 3.0f);
    Vec3 c = a - b;
    EXPECT_FLOAT_EQ(c.x, 3.0f);
    EXPECT_FLOAT_EQ(c.y, 3.0f);
    EXPECT_FLOAT_EQ(c.z, 3.0f);
}

TEST(Vec3Test, ScalarMultiplication) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    Vec3 result = v * 2.0f;
    EXPECT_FLOAT_EQ(result.x, 2.0f);
    EXPECT_FLOAT_EQ(result.y, 4.0f);
    EXPECT_FLOAT_EQ(result.z, 6.0f);
}

TEST(Vec3Test, DotProduct) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    float dot = a.Dot(b);
    EXPECT_FLOAT_EQ(dot, 32.0f);  // 1*4 + 2*5 + 3*6
}

TEST(Vec3Test, Length) {
    Vec3 v(3.0f, 4.0f, 0.0f);
    float len = v.Length();
    EXPECT_FLOAT_EQ(len, 5.0f);
}

TEST(Vec3Test, Normalized) {
    Vec3 v(3.0f, 4.0f, 0.0f);
    Vec3 n = v.Normalized();
    EXPECT_FLOAT_EQ(n.Length(), 1.0f);
}

TEST(Vec3Test, Equality) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(1.0f, 2.0f, 3.0f);
    Vec3 c(1.0f, 2.0f, 4.0f);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

// ======== ================== ==== ========= ============================
// Quat 测试
// ======== ================== ==== ========= ============================

TEST(QuatTest, DefaultConstructor) {
    Quat q;
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
    EXPECT_FLOAT_EQ(q.w, 1.0f);  // 单位四元数
}

TEST(QuatTest, Identity) {
    Quat q = Quat::Identity();
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
    EXPECT_FLOAT_EQ(q.w, 1.0f);
}

TEST(QuatTest, Length) {
    Quat q(0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(q.Length(), 1.0f);
}

TEST(QuatTest, Conjugate) {
    Quat q(0.1f, 0.2f, 0.3f, 0.9f);
    Quat conj = q.Conjugate();
    EXPECT_FLOAT_EQ(conj.x, -0.1f);
    EXPECT_FLOAT_EQ(conj.y, -0.2f);
    EXPECT_FLOAT_EQ(conj.z, -0.3f);
    EXPECT_FLOAT_EQ(conj.w, 0.9f);
}

// ======== ================== ==== ========= ============================
// PhysicsObjectState 测试
// ======== ================== ==== ========= ============================

TEST(PhysicsObjectStateTest, DefaultConstruction) {
    PhysicsObjectState state;
    EXPECT_EQ(state.objectId, 0);
    EXPECT_EQ(state.flags, 0);
    EXPECT_EQ(state.version, 0);
}

TEST(PhysicsObjectStateTest, PositionRotation) {
    PhysicsObjectState state;
    state.objectId = 1;
    state.position = Vec3(1.0f, 2.0f, 3.0f);
    state.rotation = Quat::Identity();
    
    EXPECT_EQ(state.objectId, 1);
    EXPECT_FLOAT_EQ(state.position.x, 1.0f);
    EXPECT_FLOAT_EQ(state.rotation.w, 1.0f);
}

TEST(PhysicsObjectStateTest, SerializationSize) {
    PhysicsObjectState state;
    constexpr size_t expectedSize = sizeof(uint32_t) +        // objectId
                                    sizeof(uint8_t) +         // flags
                                    sizeof(uint32_t) +        // version
                                    sizeof(float) * 3 +       // position
                                    sizeof(float) * 4 +       // rotation
                                    sizeof(float) * 3 +       // linearVelocity
                                    sizeof(float) * 3 +       // angularVelocity
                                    sizeof(float) * 3 +       // gravityScale (deprecated)
                                    sizeof(float) +           // mass
                                    sizeof(float);            // friction
    
    EXPECT_EQ(state.CalculateSerializedSize(), expectedSize);
}

TEST(PhysicsObjectStateTest, FullSerialization) {
    PhysicsObjectState original;
    original.objectId = 42;
    original.flags = 0x03;
    original.version = 1;
    original.position = Vec3(1.0f, 2.0f, 3.0f);
    original.rotation = Quat::Identity();
    original.linearVelocity = Vec3(10.0f, 0.0f, 0.0f);
    original.angularVelocity = Vec3(0.0f, 0.0f, 0.0f);
    original.mass = 1.5f;
    original.friction = 0.8f;
    
    std::array<uint8_t, 256> buffer;
    auto* serializer = Serializer::Create(buffer.data(), buffer.size());
    
    size_t size = original.Serialize(serializer);
    EXPECT_GT(size, 0);
    
    // 反序列化
    Serializer deserializer(buffer.data(), size);
    PhysicsObjectState deserialized;
    bool success = deserialized.Deserialize(&deserializer);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(deserialized.objectId, original.objectId);
    EXPECT_EQ(deserialized.flags, original.flags);
    EXPECT_EQ(deserialized.version, original.version);
    EXPECT_FLOAT_EQ(deserialized.position.x, original.position.x);
    EXPECT_FLOAT_EQ(deserialized.position.y, original.position.y);
    EXPECT_FLOAT_EQ(deserialized.position.z, original.position.z);
    EXPECT_FLOAT_EQ(deserialized.linearVelocity.x, original.linearVelocity.x);
    EXPECT_FLOAT_EQ(deserialized.mass, original.mass);
    EXPECT_FLOAT_EQ(deserialized.friction, original.friction);
    
    Serializer::Destroy(serializer);
}

// ======== ================== ==== ========= ============================
// Serializer 测试
// ======== ================== ==== ========= ============================

TEST(SerializerTest, BasicWriteRead) {
    std::array<uint8_t, 256> buffer;
    auto* serializer = Serializer::Create(buffer.data(), buffer.size());
    
    // 写入基本类型
    serializer->Write<uint32_t>(42);
    serializer->Write<float>(3.14f);
    serializer->Write<int16_t>(-100);
    
    // 读取并验证
    serializer->Seek(0, Serializer::SeekOrigin::kStart);
    
    uint32_t u32 = serializer->Read<uint32_t>();
    float f = serializer->Read<float>();
    int16_t i16 = serializer->Read<int16_t>();
    
    EXPECT_EQ(u32, 42);
    EXPECT_FLOAT_EQ(f, 3.14f);
    EXPECT_EQ(i16, -100);
    
    Serializer::Destroy(serializer);
}

TEST(SerializerTest, VectorSerialization) {
    std::array<uint8_t, 256> buffer;
    auto* serializer = Serializer::Create(buffer.data(), buffer.size());
    
    std::array<float, 4> data = {1.0f, 2.0f, 3.0f, 4.0f};
    serializer->WriteVec3(data[0], data[1], data[2]);
    
    serializer->Seek(0, Serializer::SeekOrigin::kStart);
    float r0, r1, r2;
    serializer->ReadVec3(&r0, &r1, &r2);
    
    EXPECT_FLOAT_EQ(r0, 1.0f);
    EXPECT_FLOAT_EQ(r1, 2.0f);
    EXPECT_FLOAT_EQ(r2, 3.0f);
    
    Serializer::Destroy(serializer);
}

TEST(SerializerTest, BufferOverflow) {
    std::array<uint8_t, 4> buffer;
    auto* serializer = Serializer::Create(buffer.data(), buffer.size());
    
    // 尝试写入超过缓冲区大小的数据
    uint64_t largeValue = 0x123456789ABCDEF0ULL;
    serializer->Write<uint64_t>(largeValue);
    
    // 应该检测到溢出并返回false
    EXPECT_FALSE(serializer->Commit());
    
    Serializer::Destroy(serializer);
}

TEST(SerializerTest, StreamWriteRead) {
    std::array<uint8_t, 256> buffer;
    
    // 使用 Stream 模式写入
    {
        auto* serializer = Serializer::Create(buffer.data(), buffer.size());
        serializer->BeginStream();
        
        uint32_t values[] = {10, 20, 30, 40, 50};
        for (auto v : values) {
            serializer->Write<uint32_t>(v);
        }
        
        serializer->EndStream();
        Serializer::Destroy(serializer);
    }
    
    // 读取并验证
    auto* reader = Serializer::Create(buffer.data(), buffer.size());
    std::vector<uint32_t> results;
    
    while (!reader->IsEof()) {
        results.push_back(reader->Read<uint32_t>());
    }
    
    EXPECT_EQ(results.size(), 5u);
    EXPECT_EQ(results[0], 10);
    EXPECT_EQ(results[4], 50);
    
    Serializer::Destroy(reader);
}

// ======== ================== ==== ========= ============================
// TimeStepManager 测试
// ======== ================== ==== ========= ============================

TEST(TimeStepManagerTest, Constructor) {
    TimeStepManager manager;
    EXPECT_EQ(manager.GetFixedDeltaTime(), 1.0f / 60.0f);
    EXPECT_EQ(manager.GetMaxSubSteps(), 8);
    EXPECT_EQ(manager.GetAccumulatedTime(), 0.0);
}

TEST(TimeStepManagerTest, StepCalculation) {
    TimeStepManager manager(1.0f / 60.0f, 8);
    
    // 模拟时间流逝
    double elapsed = 0.0;
    int fixedSteps = 0;
    
    for (int i = 0; i < 10; i++) {
        manager.AdvanceTime(elapsed);
        while (manager.NeedFixedStep(elapsed)) {
            manager.BeginFixedStep();
            fixedSteps++;
            manager.EndFixedStep();
        }
        elapsed += 1.0 / 60.0;
    }
    
    // 10帧 * 60fps ≈ 10个固定步长
    EXPECT_GT(fixedSteps, 5);
}

// ======== ================== ==== ========= ============================
// DeterministicRandom 测试
// ======== ================== ==== ========= ============================

TEST(DeterministicRandomTest, SameSeedSameResult) {
    DeterministicRandom rng1(12345);
    DeterministicRandom rng2(12345);
    
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(rng1.NextInt(0, 1000), rng2.NextInt(0, 1000));
    }
}

TEST(DeterministicRandomTest, DifferentSeedDifferentResult) {
    DeterministicRandom rng1(12345);
    DeterministicRandom rng2(54321);
    
    bool different = false;
    for (int i = 0; i < 100; i++) {
        if (rng1.NextInt(0, 1000) != rng2.NextInt(0, 1000)) {
            different = true;
            break;
        }
    }
    
    EXPECT_TRUE(different);
}

TEST(DeterministicRandomTest, RangeBounds) {
    DeterministicRandom rng(42);
    
    for (int i = 0; i < 1000; i++) {
        int value = rng.NextInt(10, 20);
        EXPECT_GE(value, 10);
        EXPECT_LE(value, 20);
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

TEST(DeterministicRandomTest, Probability) {
    DeterministicRandom rng(42);
    
    int heads = 0;
    int total = 1000;
    
    for (int i = 0; i < total; i++) {
        if (rng.Probability(0.5f)) {
            heads++;
        }
    }
    
    // 50% 概率，结果应该接近 500
    EXPECT_GE(heads, total * 0.3f);
    EXPECT_LE(heads, total * 0.7f);
}

TEST(DeterministicRandomTest, Shuffle) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    {
        DeterministicRandom rng(12345);
        rng.Shuffle(data.begin(), data.end());
        std::vector<int> result1 = data;
        
        rng.Shuffle(data.begin(), data.end());
    }
    
    // 重新洗牌
    data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    {
        DeterministicRandom rng(12345);
        rng.Shuffle(data.begin(), data.end());
        std::vector<int> result2 = data;
        
        EXPECT_EQ(result1, result2);  // 相同种子应产生相同结果
    }
}

// ======== ================== ==== ========= ============================
// NetworkProtocol 测试
// ======== ================== ==== ========= ============================

TEST(NetworkProtocolTest, MessageTypes) {
    // 验证消息类型ID连续性
    EXPECT_LT(static_cast<int>(MessageType::kHandshakeRequest), 
              static_cast<int>(MessageType::kMax));
    EXPECT_LT(static_cast<int>(MessageType::kHandshakeResponse), 
              static_cast<int>(MessageType::kMax));
    EXPECT_LT(static_cast<int>(MessageType::kPhysicsSnapshot), 
              static_cast<int>(MessageType::kMax));
    EXPECT_LT(static_cast<int>(MessageType::kPlayerInput), 
              static_cast<int>(MessageType::kMax));
}

TEST(NetworkProtocolTest, HandshakeMessageSizes) {
    EXPECT_EQ(sizeof(HandshakeRequest), 8);  // version + maxObjects + padding
    EXPECT_EQ(sizeof(HandshakeResponse), 12);  // result + serverVersion + maxObjects + padding
}

TEST(NetworkProtocolTest, PlayerInputSize) {
    // 验证玩家输入消息大小
    constexpr size_t expected = sizeof(uint32_t) +    // playerId
                                sizeof(uint32_t) +    // frame
                                sizeof(float) * 3 +   // cameraRotation (方向)
                                sizeof(float) * 3 +   // movement (移动)
                                sizeof(uint32_t);     // actions (动作掩码)
    
    EXPECT_EQ(sizeof(PlayerInput), expected);
}

TEST(NetworkProtocolTest, PhysicsSnapshotHeaderSize) {
    EXPECT_EQ(sizeof(PhysicsSnapshotHeader), 24);  // tick + numObjects + flags + padding
}

// ======== ================== ==== ========= ============================
// 综合测试：完整通信流程模拟
// ======== ================== ==== ========= ============================

TEST(IntegrationTest, SnapshotSerialization) {
    // 创建快照
    PhysicsWorldSnapshot snapshot;
    snapshot.tickNumber = 100;
    snapshot.gravity = Vec3(0.0f, -9.81f, 0.0f);
    
    // 添加一些物体
    for (int i = 0; i < 10; i++) {
        PhysicsObjectState obj;
        obj.objectId = i;
        obj.position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
        obj.rotation = Quat::Identity();
        obj.linearVelocity = Vec3(0.0f, 0.0f, 0.0f);
        obj.angularVelocity = Vec3(0.0f, 0.0f, 0.0f);
        obj.mass = 1.0f;
        obj.friction = 0.5f;
        obj.flags = 0;
        obj.version = 1;
        snapshot.objects.push_back(obj);
    }
    
    // 序列化
    std::array<uint8_t, 4096> buffer;
    auto* serializer = Serializer::Create(buffer.data(), buffer.size());
    
    size_t serializedSize = snapshot.Serialize(serializer);
    EXPECT_GT(serializedSize, 0);
    EXPECT_LT(serializedSize, buffer.size());
    
    // 反序列化
    Serializer deserializer(buffer.data(), serializedSize);
    PhysicsWorldSnapshot deserialized;
    bool success = deserialized.Deserialize(&deserializer);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(deserialized.tickNumber, snapshot.tickNumber);
    EXPECT_EQ(deserialized.objects.size(), snapshot.objects.size());
    
    // 验证每个物体的数据
    for (size_t i = 0; i < snapshot.objects.size(); i++) {
        EXPECT_EQ(deserialized.objects[i].objectId, snapshot.objects[i].objectId);
        EXPECT_FLOAT_EQ(deserialized.objects[i].position.x, 
                       snapshot.objects[i].position.x);
        EXPECT_FLOAT_EQ(deserialized.objects[i].mass, 
                       snapshot.objects[i].mass);
    }
    
    Serializer::Destroy(serializer);
}

// 主函数入口
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
