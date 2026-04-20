/**
 * @file common_tests.cpp
 * @brief 公共库单元测试 - 匹配实际API
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "physics_state.h"
#include "network_protocol.h"
#include "timestep_manager.h"
#include "deterministic_random.h"

using namespace PhysicsSync;

// ======== Vec3 测试 ========

TEST(Vec3Test, DefaultConstructor) {
    Vec3 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vec3Test, Addition) {
    Vec3 a; a.x = 1.0f; a.y = 2.0f; a.z = 3.0f;
    Vec3 b; b.x = 4.0f; b.y = 5.0f; b.z = 6.0f;
    Vec3 c = a + b;
    EXPECT_FLOAT_EQ(c.x, 5.0f);
    EXPECT_FLOAT_EQ(c.y, 7.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);
}

TEST(Vec3Test, DotProduct) {
    Vec3 a; a.x = 1.0f; a.y = 2.0f; a.z = 3.0f;
    Vec3 b; b.x = 4.0f; b.y = 5.0f; b.z = 6.0f;
    EXPECT_FLOAT_EQ(a.Dot(b), 32.0f);
}

TEST(Vec3Test, Normalized) {
    Vec3 v; v.x = 3.0f; v.y = 4.0f; v.z = 0.0f;
    Vec3 n = v.Normalized();
    EXPECT_FLOAT_EQ(n.Length(), 1.0f);
}

TEST(Vec3Test, Serialization) {
    Vec3 v; v.x = 1.5f; v.y = -2.5f; v.z = 3.5f;
    std::vector<uint8_t> buf;
    v.Serialize(buf);
    EXPECT_EQ(buf.size(), 12u);

    const uint8_t* p = buf.data();
    Vec3 v2; v2.x = v2.y = v2.z = 0.0f;
    v2.Deserialize(p);
    EXPECT_FLOAT_EQ(v.x, v2.x);
    EXPECT_FLOAT_EQ(v.y, v2.y);
    EXPECT_FLOAT_EQ(v.z, v2.z);
}

// ======== Quat 测试 ========

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
    Quat rotation; rotation.x = 0.0f; rotation.y = 0.0f; rotation.z = 0.7071f; rotation.w = 0.7071f;
    Quat halfway = identity.Slerp(rotation, 0.5f);
    EXPECT_NEAR(halfway.w, 0.9239f, 0.01f);
}

TEST(QuatTest, Serialization) {
    Quat q; q.x = 0.1f; q.y = 0.2f; q.z = 0.3f; q.w = 0.9f;
    std::vector<uint8_t> buf;
    q.Serialize(buf);
    EXPECT_EQ(buf.size(), 16u);

    const uint8_t* p = buf.data();
    Quat q2; q2.x = q2.y = q2.z = 0.0f; q2.w = 0.0f;
    q2.Deserialize(p);
    EXPECT_FLOAT_EQ(q.x, q2.x);
    EXPECT_FLOAT_EQ(q.y, q2.y);
    EXPECT_FLOAT_EQ(q.z, q2.z);
    EXPECT_FLOAT_EQ(q.w, q2.w);
}

// ======== PhysicsObjectState 测试 ========

TEST(PhysicsObjectStateTest, DefaultConstruction) {
    PhysicsObjectState state;
    EXPECT_EQ(state.objectId, 0u);
    EXPECT_EQ(state.type, PhysicsObjectType::DYNAMIC);
    EXPECT_FLOAT_EQ(state.position.x, 0.0f);
}

TEST(PhysicsObjectStateTest, Constructor) {
    Vec3 pos; pos.x = 1.0f; pos.y = 2.0f; pos.z = 3.0f;
    PhysicsObjectState state(1, pos, Quat::Identity(), PhysicsObjectType::PLAYER);
    EXPECT_EQ(state.objectId, 1u);
    EXPECT_EQ(state.type, PhysicsObjectType::PLAYER);
    EXPECT_FLOAT_EQ(state.position.x, 1.0f);
}

TEST(PhysicsObjectStateTest, SerializeDeserialize) {
    Vec3 pos; pos.x = 1.0f; pos.y = 2.0f; pos.z = 3.0f;
    PhysicsObjectState original(1, pos, Quat::Identity(), PhysicsObjectType::PLAYER);
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
    Vec3 pos; pos.x = 0.0f; pos.y = 0.0f; pos.z = 0.0f;
    PhysicsObjectState state(1, pos, Quat::Identity(), PhysicsObjectType::DYNAMIC);
    state.linearVelocity = Vec3{2.0f, 0.0f, 0.0f};
    Vec3 predicted = state.PredictPosition(0.5f);
    EXPECT_FLOAT_EQ(predicted.x, 1.0f);
}

// ======== PhysicsWorldSnapshot 测试 ========

TEST(PhysicsWorldSnapshotTest, DefaultConstruction) {
    PhysicsWorldSnapshot snapshot;
    EXPECT_EQ(snapshot.objectCount, 0u);
    EXPECT_EQ(snapshot.objects.size(), 0u);
}

TEST(PhysicsWorldSnapshotTest, AddObject) {
    PhysicsWorldSnapshot snapshot;
    Vec3 pos; pos.x = 1.0f; pos.y = 2.0f; pos.z = 3.0f;
    snapshot.AddObject(PhysicsObjectState(1, pos, Quat::Identity(), PhysicsObjectType::PLAYER));
    EXPECT_EQ(snapshot.objects.size(), 1u);
    EXPECT_EQ(snapshot.objectCount, 1u);
}

TEST(PhysicsWorldSnapshotTest, FindObject) {
    PhysicsWorldSnapshot snapshot;
    Vec3 pos; pos.x = 1.0f; pos.y = 2.0f; pos.z = 3.0f;
    snapshot.AddObject(PhysicsObjectState(1, pos, Quat::Identity(), PhysicsObjectType::PLAYER));
    auto* obj = snapshot.FindObject(1);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->objectId, 1u);

    auto* notFound = snapshot.FindObject(999);
    EXPECT_EQ(notFound, nullptr);
}

TEST(PhysicsWorldSnapshotTest, RemoveObject) {
    PhysicsWorldSnapshot snapshot;
    Vec3 pos; pos.x = 1.0f; pos.y = 2.0f; pos.z = 3.0f;
    snapshot.AddObject(PhysicsObjectState(1, pos, Quat::Identity(), PhysicsObjectType::PLAYER));
    snapshot.AddObject(PhysicsObjectState(2, pos, Quat::Identity(), PhysicsObjectType::DYNAMIC));

    EXPECT_TRUE(snapshot.RemoveObject(1));
    EXPECT_EQ(snapshot.objects.size(), 1u);
    EXPECT_EQ(snapshot.objectCount, 1u);

    EXPECT_FALSE(snapshot.RemoveObject(999));
}

// ======== NetworkMessage 测试 ========

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

// ======== TimeStepManager 测试 ========

TEST(TimeStepManagerTest, FixedStepTiming) {
    TimeStepManager manager;
    manager.SetFixedTimeStep(1.0f / 60.0f);

    for (int i = 0; i < 10; i++) {
        manager.StartNewFrame(1.0f / 60.0f);
        while (manager.ShouldTick()) {
            manager.Tick();
        }
        manager.FinishFrame();
    }

    EXPECT_EQ(manager.GetCurrentTick(), 10u);
}

TEST(TimeStepManagerTest, DeltaClamp) {
    TimeStepManager manager;
    manager.SetFixedTimeStep(1.0f / 60.0f);

    manager.StartNewFrame(1.0f);
    while (manager.ShouldTick()) {
        manager.Tick();
    }
    manager.FinishFrame();

    EXPECT_LE(manager.GetCurrentTick(), 60u);
}

// ======== DeterministicRandom 测试 ========

TEST(DeterministicRandomTest, Reproducibility) {
    DeterministicRandom rng1(42);
    DeterministicRandom rng2(42);

    std::vector<int> seq1, seq2;
    for (int i = 0; i < 100; i++) {
        seq1.push_back(static_cast<int>(rng1.Next(0, 1000)));
        seq2.push_back(static_cast<int>(rng2.Next(0, 1000)));
    }

    EXPECT_EQ(seq1, seq2);
}

TEST(DeterministicRandomTest, Range) {
    DeterministicRandom rng(12345);

    for (int i = 0; i < 1000; i++) {
        int val = static_cast<int>(rng.Next(10, 20));
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

// ======== MessageFactory 测试 ========

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

// ======== 运行所有测试 ========

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
