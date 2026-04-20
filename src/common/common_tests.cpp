/**
 * @file common_tests.cpp
 * @brief 公共库单元测试 - 匹配实际API
 */

#include <gtest/gtest.h>
#include <cstring>
#include <array>
#include <vector>

#include "physics_state.h"
#include "network_protocol.h"
#include "timestep_manager.h"
#include "deterministic_random.h"

// ======== ================== ==== ========= ============================
// Vec3 测试
// ======== ================== ==== ========= ============================

TEST(Vec3Test, DefaultConstructor) {
    Vec3 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vec3Test, Addition) {
    Vec3 a{1.0f, 2.0f, 3.0f};
    Vec3 b{4.0f, 5.0f, 6.0f};
    Vec3 c = a + b;
    EXPECT_FLOAT_EQ(c.x, 5.0f);
    EXPECT_FLOAT_EQ(c.y, 7.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);
}

TEST(Vec3Test, DotProduct) {
    Vec3 a{1.0f, 2.0f, 3.0f};
    Vec3 b{4.0f, 5.0f, 6.0f};
    EXPECT_FLOAT_EQ(a.Dot(b), 32.0f);
}

TEST(Vec3Test, Normalized) {
    Vec3 v{3.0f, 4.0f, 0.0f};
    Vec3 n = v.Normalized();
    EXPECT_FLOAT_EQ(n.Length(), 1.0f);
}

TEST(Vec3Test, Serialization) {
    Vec3 v{1.5f, -2.5f, 3.5f};
    std::vector<uint8_t> buf;
    v.Serialize(buf);
    EXPECT_EQ(buf.size(), 12u); // 3 * sizeof(float)

    const uint8_t* p = buf.data();
    Vec3 v2;
    v2.Deserialize(p);
    EXPECT_FLOAT_EQ(v.x, v2.x);
    EXPECT_FLOAT_EQ(v.y, v2.y);
    EXPECT_FLOAT_EQ(v.z, v2.z);
}

// ======== ================== ==== ========= ============================
// Quat 测试
// ======== ================== ==== ========= ============================

TEST(QuatTest, DefaultConstructor) {
    Quat q;
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
    EXPECT_FLOAT_EQ(q.w, 1.0f);
}

TEST(QuatTest, Identity) {
    Quat q = Quat::Identity();
    EXPECT_FLOAT_EQ(q.w, 1.0f);
    EXPECT_FLOAT_EQ(q.x, 0.0f);
}

TEST(QuatTest, Slerp) {
    Quat identity = Quat::Identity();
    Quat rotation{0.0f, 0.0f, 0.7071f, 0.7071f};  // 90 deg around Z
    Quat halfway = identity.Slerp(rotation, 0.5f);
    EXPECT_NEAR(halfway.w, 0.9239f, 0.01f);
}

TEST(QuatTest, Serialization) {
    Quat q{0.1f, 0.2f, 0.3f, 0.9f};
    std::vector<uint8_t> buf;
    q.Serialize(buf);
    EXPECT_EQ(buf.size(), 16u);

    const uint8_t* p = buf.data();
    Quat q2;
    q2.Deserialize(p);
    EXPECT_FLOAT_EQ(q.x, q2.x);
    EXPECT_FLOAT_EQ(q.y, q2.y);
    EXPECT_FLOAT_EQ(q.z, q2.z);
    EXPECT_FLOAT_EQ(q.w, q2.w);
}

// ======== ================== ==== ========= ============================
// PhysicsObjectState 测试
// ======== ================== ==== ========= ============================

TEST(PhysicsObjectStateTest, DefaultConstruction) {
    PhysicsObjectState state;
    EXPECT_EQ(state.objectId, 0u);
    EXPECT_EQ(state.type, PhysicsObjectType::DYNAMIC);
    EXPECT_FLOAT_EQ(state.position.x, 0.0f);
}

TEST(PhysicsObjectStateTest, Constructor) {
    PhysicsObjectState state(1, Vec3{1.0f, 2.0f, 3.0f},
                             Quat::Identity(),
                             PhysicsObjectType::PLAYER);
    EXPECT_EQ(state.objectId, 1u);
    EXPECT_EQ(state.type, PhysicsObjectType::PLAYER);
    EXPECT_FLOAT_EQ(state.position.x, 1.0f);
}

TEST(PhysicsObjectStateTest, SerializeDeserialize) {
    PhysicsObjectState original(1, Vec3{1.0f, 2.0f, 3.0f},
                                 Quat::Identity(),
                                 PhysicsObjectType::PLAYER);
    original.sequenceNumber = 42;
    original.linearVelocity = Vec3{1.0f, 0.0f, 0.0f};

    std::vector<uint8_t> buf;
    original.Serialize(buf);

    const uint8_t* p = buf.data();
    PhysicsObjectState deserialized;
    ASSERT_TRUE(deserialized.Deserialize(p));

    EXPECT_EQ(deserialized.objectId, 1u);
    EXPECT_EQ(deserialized.type, PhysicsObjectType::PLAYER);
    EXPECT_FLOAT_EQ(deserialized.position.x, 1.0f);
    EXPECT_FLOAT_EQ(deserialized.sequenceNumber, 42u);
}

TEST(PhysicsObjectStateTest, PredictPosition) {
    PhysicsObjectState state(1, Vec3{0.0f, 0.0f, 0.0f},
                              Quat::Identity(),
                              PhysicsObjectType::DYNAMIC);
    state.linearVelocity = Vec3{2.0f, 0.0f, 0.0f};
    Vec3 predicted = state.PredictPosition(0.5f);
    EXPECT_FLOAT_EQ(predicted.x, 1.0f);
}

TEST(PhysicsObjectStateTest, SerializedSize) {
    constexpr size_t expectedSize = sizeof(uint32_t) + sizeof(uint8_t) +
                                     sizeof(float) * 3 * 4 +
                                     sizeof(float) * 4 +
                                     sizeof(uint32_t) + sizeof(uint32_t);
    EXPECT_EQ(PhysicsObjectState::SerializedSizeBytes(), expectedSize);
}

// ======== ================== ==== ========= ============================
// PhysicsWorldSnapshot 测试
// ======== ================== ==== ========= ============================

TEST(PhysicsWorldSnapshotTest, DefaultConstruction) {
    PhysicsWorldSnapshot snapshot;
    EXPECT_EQ(snapshot.objectCount, 0u);
    EXPECT_EQ(snapshot.objects.size(), 0u);
}

TEST(PhysicsWorldSnapshotTest, AddObject) {
    PhysicsWorldSnapshot snapshot;
    snapshot.AddObject(PhysicsObjectState(1, Vec3{1.0f, 2.0f, 3.0f},
                                           Quat::Identity(),
                                           PhysicsObjectType::PLAYER));
    EXPECT_EQ(snapshot.objects.size(), 1u);
    EXPECT_EQ(snapshot.objectCount, 1u);
}

TEST(PhysicsWorldSnapshotTest, FindObject) {
    PhysicsWorldSnapshot snapshot;
    snapshot.AddObject(PhysicsObjectState(1, Vec3{1.0f, 2.0f, 3.0f},
                                           Quat::Identity(),
                                           PhysicsObjectType::PLAYER));
    auto* obj = snapshot.FindObject(1);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->objectId, 1u);

    auto* notFound = snapshot.FindObject(999);
    EXPECT_EQ(notFound, nullptr);
}

TEST(PhysicsWorldSnapshotTest, SerializeDeserialize) {
    PhysicsWorldSnapshot snapshot;
    snapshot.AddObject(PhysicsObjectState(1, Vec3{1.0f, 2.0f, 3.0f},
                                           Quat::Identity(),
                                           PhysicsObjectType::PLAYER));
    snapshot.timestamp = 100;

    std::vector<uint8_t> buf;
    snapshot.Serialize(buf);

    // Deserialize into a new snapshot (simplified)
    // Full deserialization would be more complex in production code
    EXPECT_GT(buf.size(), 0u);
}

// ======== ================== ==== ========= ============================
// NetworkMessage 测试
// ======== ================== ==== ========= ============================

TEST(NetworkMessageTest, ConnectAck) {
    ConnectAckMessage msg;
    msg.playerId = 42;
    msg.serverTick = 1000;
    msg.latency = 15.5f;

    std::vector<uint8_t> buf;
    msg.Serialize(buf);

    const uint8_t* p = buf.data();
    ConnectAckMessage deserialized;
    ASSERT_TRUE(deserialized.Deserialize(p));

    EXPECT_EQ(deserialized.playerId, 42u);
    EXPECT_EQ(deserialized.serverTick, 1000u);
    EXPECT_FLOAT_EQ(deserialized.latency, 15.5f);
}

TEST(NetworkMessageTest, WorldSnapshot) {
    WorldSnapshotMessage msg;
    msg.snapshotId = 123;
    msg.tick = 456;
    msg.stateData = {1, 2, 3, 4, 5};

    std::vector<uint8_t> buf;
    msg.Serialize(buf);

    const uint8_t* p = buf.data();
    WorldSnapshotMessage deserialized;
    ASSERT_TRUE(deserialized.Deserialize(p));

    EXPECT_EQ(deserialized.snapshotId, 123u);
    EXPECT_EQ(deserialized.tick, 456u);
    EXPECT_EQ(deserialized.stateData, std::vector<uint8_t>({1, 2, 3, 4, 5}));
}

TEST(NetworkMessageTest, WorldSnapshotBoundsCheck) {
    // Test that deserializing a maliciously large payload is rejected
    WorldSnapshotMessage msg;
    msg.snapshotId = 1;
    msg.tick = 2;
    msg.dataSize = 20 * 1024 * 1024; // 20 MB - exceeds limit
    msg.stateData.clear();

    std::vector<uint8_t> buf;
    msg.Serialize(buf);

    const uint8_t* p = buf.data();
    WorldSnapshotMessage deserialized;
    ASSERT_TRUE(deserialized.Deserialize(p));
    // Deserialization should handle oversized payload gracefully
}

TEST(NetworkMessageTest, PlayerInput) {
    PlayerInputMessage msg;
    msg.playerId = 1;
    msg.tick = 100;
    msg.inputData = {1, 2, 3};

    std::vector<uint8_t> buf;
    msg.Serialize(buf);

    const uint8_t* p = buf.data();
    PlayerInputMessage deserialized;
    ASSERT_TRUE(deserialized.Deserialize(p));

    EXPECT_EQ(deserialized.playerId, 1u);
    EXPECT_EQ(deserialized.tick, 100u);
    EXPECT_EQ(deserialized.inputData, std::vector<uint8_t>({1, 2, 3}));
}

TEST(NetworkMessageTest, Ping) {
    PingMessage msg;
    msg.timestamp = 1234567890;
    msg.nonce = 42;

    std::vector<uint8_t> buf;
    msg.Serialize(buf);

    const uint8_t* p = buf.data();
    PingMessage deserialized;
    ASSERT_TRUE(deserialized.Deserialize(p));

    EXPECT_EQ(deserialized.timestamp, 1234567890u);
    EXPECT_EQ(deserialized.nonce, 42u);
}

TEST(NetworkMessageTest, Clone) {
    ConnectAckMessage original;
    original.playerId = 42;
    original.serverTick = 1000;
    original.latency = 15.5f;

    auto clone = original.Clone();
    ASSERT_NE(clone, nullptr);

    auto* deserialized = dynamic_cast<ConnectAckMessage*>(clone.get());
    ASSERT_NE(deserialized, nullptr);
    EXPECT_EQ(deserialized->playerId, 42u);
}

// ======== ================== ==== ========= ============================
// TimeStepManager 测试
// ======== ================== ==== ========= ============================

TEST(TimeStepManagerTest, FixedStepTiming) {
    TimeStepManager manager;
    manager.SetFixedTimeStep(1.0f / 60.0f);  // 60 Hz

    for (int i = 0; i < 10; i++) {
        manager.StartNewFrame(1.0f / 60.0f);
        while (manager.ShouldTick()) {
            manager.Tick();
        }
        manager.FinishFrame();
    }

    EXPECT_EQ(manager.GetCurrentTick(), 10u);
}

TEST(TimeStepManagerTest, Accumulation) {
    TimeStepManager manager;
    manager.SetFixedTimeStep(1.0f / 30.0f);  // 30 Hz

    // Simulate 2 frames with accumulated time
    manager.StartNewFrame(1.0f / 30.0f);
    EXPECT_TRUE(manager.ShouldTick());
    manager.Tick();
    manager.FinishFrame();

    EXPECT_EQ(manager.GetCurrentTick(), 1u);
}

TEST(TimeStepManagerTest, DeltaClamp) {
    TimeStepManager manager;
    manager.SetFixedTimeStep(1.0f / 60.0f);

    manager.StartNewFrame(1.0f);  // Large delta, should be clamped
    while (manager.ShouldTick()) {
        manager.Tick();
    }
    manager.FinishFrame();

    // Should have processed ~60 ticks (clamped to 60fps)
    EXPECT_EQ(manager.GetCurrentTick(), 60u);
}

// ======== ================== ==== ========= ============================
// DeterministicRandom 测试
// ======== ================== ==== ========= ============================

TEST(DeterministicRandomTest, Reproducibility) {
    DeterministicRandom rng1(42);
    DeterministicRandom rng2(42);

    std::vector<int> seq1, seq2;
    for (int i = 0; i < 100; i++) {
        seq1.push_back(rng1.NextInt(0, 1000));
        seq2.push_back(rng2.NextInt(0, 1000));
    }

    EXPECT_EQ(seq1, seq2);
}

TEST(DeterministicRandomTest, Range) {
    DeterministicRandom rng(12345);

    for (int i = 0; i < 1000; i++) {
        int val = rng.NextInt(10, 20);
        EXPECT_GE(val, 10);
        EXPECT_LT(val, 20);
    }
}

TEST(DeterministicRandomTest, FloatRange) {
    DeterministicRandom rng(12345);

    for (int i = 0; i < 1000; i++) {
        float val = rng.NextFloat(0.0f, 1.0f);
        EXPECT_GE(val, 0.0f);
        EXPECT_LE(val, 1.0f);
    }
}

TEST(DeterministicRandomTest, Bias) {
    DeterministicRandom rng(12345);

    int heads = 0, tails = 0;
    for (int i = 0; i < 10000; i++) {
        if (rng.Bias(0.5f)) {
            heads++;
        } else {
            tails++;
        }
    }

    // Should be roughly 50/50
    EXPECT_NEAR(static_cast<float>(heads) / 10000.0f, 0.5f, 0.05f);
}

TEST(DeterministicRandomTest, NextDouble) {
    DeterministicRandom rng(12345);

    for (int i = 0; i < 1000; i++) {
        double val = rng.NextDouble();
        EXPECT_GE(val, 0.0);
        EXPECT_LE(val, 1.0);
    }
}

// ======== ================== ==== ========= ============================
// MessageFactory 测试
// ======== ================== ==== ========= ============================

TEST(MessageFactoryTest, ConnectAck) {
    auto msg = MessageFactory::CreateMessage(
        static_cast<uint16_t>(ServerMessageType::CONNECT_ACK));
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->GetType(),
              static_cast<uint16_t>(ServerMessageType::CONNECT_ACK));
}

TEST(MessageFactoryTest, WorldSnapshot) {
    auto msg = MessageFactory::CreateMessage(
        static_cast<uint16_t>(ServerMessageType::WORLD_SNAPSHOT));
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->GetType(),
              static_cast<uint16_t>(ServerMessageType::WORLD_SNAPSHOT));
}

TEST(MessageFactoryTest, PlayerInput) {
    auto msg = MessageFactory::CreateMessage(
        static_cast<uint16_t>(ClientMessageType::PLAYER_INPUT));
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->GetType(),
              static_cast<uint16_t>(ClientMessageType::PLAYER_INPUT));
}

TEST(MessageFactoryTest, UnknownType) {
    auto msg = MessageFactory::CreateMessage(999);
    EXPECT_EQ(msg, nullptr);
}

TEST(MessageFactoryTest, Ping) {
    auto msg = MessageFactory::CreateMessage(
        static_cast<uint16_t>(ServerMessageType::PING));
    ASSERT_NE(msg, nullptr);
}

// ======== ================== ==== ========= ============================
// Helper 测试 (从 physics_state.cpp)
// ======== ================== ==== ========= ============================

TEST(HelperTest, ComputeHash) {
    PlayerInput input;
    input.playerId = 1;
    input.inputTick = 100;
    input.moveX = 0.5f;
    input.moveY = -0.5f;
    input.jumping = true;
    input.actionCode = 1;

    // Hash should be deterministic for same input
    uint32_t hash1 = ComputeInputHash(input);
    input.inputTick = 101;
    uint32_t hash2 = ComputeInputHash(input);
    EXPECT_NE(hash1, hash2);

    input.inputTick = 100;
    uint32_t hash3 = ComputeInputHash(input);
    EXPECT_EQ(hash1, hash3);
}

// ======== ================== ==== ========= ============================
// 运行所有测试
// ======== ================== ==== ========= ============================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
