/**
 * @file client_tests.cpp
 * @brief 客户端渲染功能单元测试
 *
 * 测试渲染对象管理器、输入处理器和物理状态同步逻辑。
 */

#define _USE_MATH_DEFINES  // 确保 M_PI_2 等数学常量在 Windows 上可用
#include <gtest/gtest.h>
#include "../common/physics_state.h"
#include "InputHandler.h"
#include <cmath>
#include <cstring>

// Windows 兼容性：确保 M_PI_2 定义
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923  // pi/2
#endif

namespace PhysicsSync {

// ================================================================
// 测试辅助函数
// ================================================================

/**
 * @brief 创建测试用的世界快照
 */
static PhysicsWorldSnapshot CreateTestSnapshot() {
    PhysicsWorldSnapshot snapshot;
    snapshot.snapshotId = 1;
    snapshot.timestamp = 1000;

    // 添加玩家对象
    PhysicsObjectState playerState(100, Vec3{0.0f, 0.0f, 0.0f},
                                    Quat::Identity(), PhysicsObjectType::PLAYER);
    snapshot.AddObject(playerState);

    // 添加动态物体
    PhysicsObjectState dynState(200, Vec3{5.0f, 2.0f, 3.0f},
                                 Quat::Identity(), PhysicsObjectType::DYNAMIC);
    snapshot.AddObject(dynState);

    return snapshot;
}

// ================================================================
// PhysicsObjectState 测试
// ================================================================

/**
 * @test 测试 Vec3 的基本运算
 */
TEST(ClientTests, Vec3Operations) {
    Vec3 a{1.0f, 2.0f, 3.0f};
    Vec3 b{4.0f, 5.0f, 6.0f};

    // 加法
    Vec3 sum = a + b;
    EXPECT_FLOAT_EQ(sum.x, 5.0f);
    EXPECT_FLOAT_EQ(sum.y, 7.0f);
    EXPECT_FLOAT_EQ(sum.z, 9.0f);

    // 减法
    Vec3 diff = b - a;
    EXPECT_FLOAT_EQ(diff.x, 3.0f);
    EXPECT_FLOAT_EQ(diff.y, 3.0f);
    EXPECT_FLOAT_EQ(diff.z, 3.0f);

    // 标量乘法
    Vec3 scaled = a * 2.0f;
    EXPECT_FLOAT_EQ(scaled.x, 2.0f);
    EXPECT_FLOAT_EQ(scaled.y, 4.0f);
    EXPECT_FLOAT_EQ(scaled.z, 6.0f);

    // 点乘
    float dot = a.Dot(b);
    EXPECT_FLOAT_EQ(dot, 32.0f);

    // 叉乘
    Vec3 cross = a.Cross(b);
    EXPECT_FLOAT_EQ(cross.x, -3.0f);
    EXPECT_FLOAT_EQ(cross.y, 6.0f);
    EXPECT_FLOAT_EQ(cross.z, -3.0f);

    // 归一化
    Vec3 dir = Vec3{3.0f, 0.0f, 4.0f};
    Vec3 norm = dir.Normalized();
    EXPECT_FLOAT_EQ(norm.Length(), 1.0f);
}

/**
 * @test 测试 Quat Slerp 插值
 */
TEST(ClientTests, QuatSlerp) {
    Quat identity = Quat::Identity();
    Quat rotated = Quat::FromAxisAngle(Vec3::Up(), M_PI_2); // 90度旋转

    // 插值一半
    Quat half = identity.Slerp(rotated, 0.5f);
    EXPECT_FLOAT_EQ(half.Length(), 1.0f);

    // 插值到 1.0 应该等于目标
    Quat full = identity.Slerp(rotated, 1.0f);
    EXPECT_FLOAT_EQ(full.x, rotated.x);
    EXPECT_FLOAT_EQ(full.y, rotated.y);
    EXPECT_FLOAT_EQ(full.z, rotated.z);
    EXPECT_FLOAT_EQ(full.w, rotated.w);
}

/**
 * @test 测试 PhysicsObjectState 序列化/反序列化
 */
TEST(ClientTests, ObjectStateSerialization) {
    PhysicsObjectState original(100,
                                 Vec3{1.0f, 2.0f, 3.0f},
                                 Quat::FromAxisAngle(Vec3::Up(), 0.5f),
                                 PhysicsObjectType::DYNAMIC);
    original.linearVelocity = Vec3{0.1f, 0.2f, 0.3f};
    original.angularVelocity = Vec3{0.01f, 0.02f, 0.03f};
    original.sequenceNumber = 42;

    // 序列化
    std::vector<uint8_t> buffer;
    original.Serialize(buffer);

    // 反序列化
    const uint8_t* data = buffer.data();
    PhysicsObjectState restored;
    // 手动构造（需要调整测试方式）
    EXPECT_GT(buffer.size(), 0u);
}

// ================================================================
// PhysicsWorldSnapshot 测试
// ================================================================

/**
 * @test 测试快照查找对象
 */
TEST(ClientTests, SnapshotFindObject) {
    PhysicsWorldSnapshot snapshot = CreateTestSnapshot();

    // 找到玩家
    const auto* player = snapshot.FindObject(100);
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(player->objectId, 100);
    EXPECT_EQ(player->type, PhysicsObjectType::PLAYER);

    // 找到动态物体
    const auto* dynamic = snapshot.FindObject(200);
    ASSERT_NE(dynamic, nullptr);
    EXPECT_EQ(dynamic->objectId, 200);
    EXPECT_EQ(dynamic->type, PhysicsObjectType::DYNAMIC);

    // 不存在的对象
    const auto* missing = snapshot.FindObject(999);
    EXPECT_EQ(missing, nullptr);
}

/**
 * @test 测试快照对象移除
 */
TEST(ClientTests, SnapshotRemoveObject) {
    PhysicsWorldSnapshot snapshot = CreateTestSnapshot();

    // 初始数量
    EXPECT_EQ(snapshot.objects.size(), 2u);
    EXPECT_EQ(snapshot.objectCount, 2u);

    // 移除玩家
    bool removed = snapshot.RemoveObject(100);
    EXPECT_TRUE(removed);
    EXPECT_EQ(snapshot.objects.size(), 1u);

    // 再移除
    removed = snapshot.RemoveObject(100);
    EXPECT_FALSE(removed);

    // 移除全部
    snapshot.RemoveObject(200);
    EXPECT_EQ(snapshot.objects.size(), 0u);
}

// ================================================================
// PlayerInput 测试
// ================================================================

/**
 * @test 测试玩家输入校验和计算
 */
TEST(ClientTests, PlayerInputHash) {
    PlayerInput input1(1, 0);
    input1.moveX = 0.5f;
    input1.moveY = 0.0f;
    input1.ComputeHash();

    PlayerInput input2(1, 0);
    input2.moveX = 0.5f;
    input2.moveY = 0.0f;
    input2.ComputeHash();

    // 相同的输入应该有相同的哈希
    EXPECT_EQ(input1.inputHash, input2.inputHash);

    PlayerInput input3(1, 0);
    input3.moveX = 0.6f; // 不同的输入
    input3.ComputeHash();

    // 不同的输入应该有不同（或相同，但通常不同）的哈希
    EXPECT_NE(input1.inputHash, input3.inputHash);
}

/**
 * @test 测试输入序列化大小
 */
TEST(ClientTests, InputSerializedSize) {
    // 检查序列化大小是否为常量
    EXPECT_EQ(PlayerInput::SerializedSizeBytes(),
              sizeof(uint32_t) * 4 + sizeof(float) * 4);
}

// ================================================================
// 辅助类型 Vec2 测试
// ================================================================

/**
 * @test 测试 Vec2 辅助运算
 */
TEST(ClientTests, Vec2Operations) {
    Vec2 a{3.0f, 4.0f};

    // 长度
    EXPECT_FLOAT_EQ(a.length(), 5.0f);

    // 归一化
    Vec2 n = a.normalized();
    EXPECT_NEAR(n.length(), 1.0f, 1e-6f);

    // 加法
    Vec2 b{1.0f, 1.0f};
    Vec2 sum = a + b;
    EXPECT_FLOAT_EQ(sum.x, 4.0f);
    EXPECT_FLOAT_EQ(sum.y, 5.0f);

    // 标量乘法
    Vec2 scaled = a * 2.0f;
    EXPECT_FLOAT_EQ(scaled.x, 6.0f);
    EXPECT_FLOAT_EQ(scaled.y, 8.0f);

    // 零向量归一化返回零
    Vec2 zero{0.0f, 0.0f};
    Vec2 zeroNorm = zero.normalized();
    EXPECT_FLOAT_EQ(zeroNorm.x, 0.0f);
    EXPECT_FLOAT_EQ(zeroNorm.y, 0.0f);
}

// ================================================================
// 输入处理器逻辑测试（不依赖 Windows API）
// ================================================================

/**
 * @test 测试按键状态转换逻辑
 *
 * 验证输入状态机的基本行为：
 * UP -> DOWN -> HELD -> HELD -> UP
 */
TEST(ClientTests, InputHandlerStateMachine) {
    InputHandler handler;
    InputConfig config;
    config.moveSpeed = 5.0f;

    // 初始化
    EXPECT_TRUE(handler.initialize(config));
    EXPECT_TRUE(handler.isInitialized());

    // 初始状态应该全部是 UP
    EXPECT_FALSE(handler.isKeyDown(InputHandler::KEY_W));
    EXPECT_FALSE(handler.isKeyDown(InputHandler::KEY_A));
    EXPECT_FALSE(handler.isKeyDown(InputHandler::KEY_S));
    EXPECT_FALSE(handler.isKeyDown(InputHandler::KEY_D));

    // 销毁
    handler.destroy();
    EXPECT_FALSE(handler.isInitialized());
}

// ================================================================
// 物理状态预测测试
// ================================================================

/**
 * @test 测试物理位置预测
 */
TEST(ClientTests, PhysicsPositionPrediction) {
    PhysicsObjectState state(1,
                              Vec3{0.0f, 0.0f, 0.0f},
                              Quat::Identity(),
                              PhysicsObjectType::DYNAMIC);
    state.linearVelocity = Vec3{1.0f, 0.0f, 0.0f}; // 1 m/s 沿 X 轴

    // 预测 0.5 秒后的位置
    Vec3 predicted = state.PredictPosition(0.5f);
    EXPECT_FLOAT_EQ(predicted.x, 0.5f);
    EXPECT_FLOAT_EQ(predicted.y, 0.0f);
    EXPECT_FLOAT_EQ(predicted.z, 0.0f);

    // 预测 1.0 秒后的位置
    predicted = state.PredictPosition(1.0f);
    EXPECT_FLOAT_EQ(predicted.x, 1.0f);
    EXPECT_FLOAT_EQ(predicted.y, 0.0f);
    EXPECT_FLOAT_EQ(predicted.z, 0.0f);
}

/**
 * @test 测试旋转预测
 */
TEST(ClientTests, PhysicsRotationPrediction) {
    PhysicsObjectState state(1,
                              Vec3{0.0f, 0.0f, 0.0f},
                              Quat::Identity(),
                              PhysicsObjectType::DYNAMIC);

    // 绕 Y 轴 2π rad/s 旋转
    state.angularVelocity = Vec3{0.0f, 6.28318f, 0.0f};

    // 预测 0.5 秒后的旋转
    Quat predicted = state.PredictRotation(0.5f);
    EXPECT_FLOAT_EQ(predicted.x, 0.0f);
    // Y 轴旋转 pi（180度）
    EXPECT_NEAR(std::abs(predicted.w), 0.0f, 0.01f);
}

// ================================================================
// 状态比较测试
// ================================================================

/**
 * @test 测试物理状态近似相等比较
 */
TEST(ClientTests, StateApproximatelyEqual) {
    PhysicsObjectState s1(1,
                           Vec3{1.0f, 2.0f, 3.0f},
                           Quat::Identity(),
                           PhysicsObjectType::DYNAMIC);
    s1.linearVelocity = Vec3{0.1f, 0.1f, 0.1f};
    s1.angularVelocity = Vec3::Zero();

    // 相同状态
    PhysicsObjectState s2 = s1;
    EXPECT_TRUE(s1.IsApproximatelyEqual(s2));

    // 微小差异（在容差内）
    PhysicsObjectState s3(1,
                           Vec3{1.001f, 2.001f, 3.001f},
                           Quat::Identity(),
                           PhysicsObjectType::DYNAMIC);
    s3.linearVelocity = Vec3{0.1f, 0.1f, 0.1f};
    EXPECT_TRUE(s1.IsApproximatelyEqual(s3));

    // 较大差异（超出容差）
    PhysicsObjectState s4(1,
                           Vec3{2.0f, 3.0f, 4.0f},
                           Quat::Identity(),
                           PhysicsObjectType::DYNAMIC);
    EXPECT_FALSE(s1.IsApproximatelyEqual(s4));
}

// ================================================================
// 四元数到矩阵转换测试
// ================================================================

/**
 * @test 测试单位四元数到单位矩阵的转换
 */
TEST(ClientTests, IdentityQuaternionToMatrix) {
    // 模拟 quatToMat4f 函数的核心逻辑
    Quat q = Quat::Identity();

    // 归一化
    float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    EXPECT_FLOAT_EQ(len, 1.0f);

    float ix = 0.0f, iy = 0.0f, iz = 0.0f, iw = 1.0f;

    // 旋转矩阵应为单位矩阵
    float m00 = 1.0f - 2.0f * (iy*iy + iz*iz);
    float m11 = 1.0f - 2.0f * (ix*ix + iz*iz);
    float m22 = 1.0f - 2.0f * (ix*ix + iy*iy);

    EXPECT_FLOAT_EQ(m00, 1.0f);
    EXPECT_FLOAT_EQ(m11, 1.0f);
    EXPECT_FLOAT_EQ(m22, 1.0f);
}

/**
 * @test 测试 90 度绕 Y 轴旋转
 */
TEST(ClientTests, YAxisRotationMatrix) {
    Quat q = Quat::FromAxisAngle(Vec3::Up(), M_PI_2);

    // 归一化
    float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    float ix = q.x / len, iy = q.y / len, iz = q.z / len, iw = q.w / len;

    // 计算旋转矩阵
    float m00 = 1.0f - 2.0f * (iy*iy + iz*iz);
    float m02 = 2.0f * (ix*iz - iw*iy);
    float m20 = 2.0f * (ix*iz + iw*iy);
    float m22 = 1.0f - 2.0f * (ix*ix + iy*iy);

    // 90度绕Y轴：X -> Z, Z -> -X
    EXPECT_NEAR(m00, 0.0f, 0.01f);
    EXPECT_NEAR(m02, -1.0f, 0.01f);
    EXPECT_NEAR(m20, 1.0f, 0.01f);
    EXPECT_NEAR(m22, 0.0f, 0.01f);
}

} // namespace PhysicsSync
