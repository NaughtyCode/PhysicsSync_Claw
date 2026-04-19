/**
 * @file deterministic_random.h
 * @brief 确定性随机数生成器
 * 
 * 本模块提供跨平台一致的伪随机数生成器，用于物理模拟和游戏逻辑。
 * 相同的种子在相同的调用序列下会产生完全相同的结果。
 * 
 * 使用 PCG32 算法，具有良好的统计特性和跨平台一致性。
 */

#pragma once

#include <cstdint>
#include <stdexcept>

namespace PhysicsSync {

// ================================================================
// PCG32 确定性随机数生成器
// ================================================================

/**
 * @brief PCG32 确定性随机数生成器
 * 
 * PCG (Permuted Congruential Generator) 是一种高质量的伪随机数生成器。
 * 我们使用确定性实现，确保跨平台一致性。
 * 
 * @note 此实现经过修改以在所有平台上产生相同的结果
 */
class DeterministicRandom {
public:
    /**
     * @brief 默认构造函数（使用固定种子）
     */
    DeterministicRandom();

    /**
     * @brief 构造函数
     * @param seed 种子值
     * @param stream 流选择器（用于生成不同的序列）
     */
    DeterministicRandom(uint64_t seed, uint64_t stream = 0);

    /**
     * @brief 析构函数
     */
    ~DeterministicRandom() = default;

    /**
     * @brief 生成一个32位随机数
     * @return 0 到 UINT32_MAX 之间的随机数
     */
    uint32_t Next();

    /**
     * @brief 生成一个范围内的随机数
     * @param min 最小值（包含）
     * @param max 最大值（包含）
     * @return min 到 max 之间的随机数
     */
    uint32_t Next(uint32_t min, uint32_t max);

    /**
     * @brief 生成一个浮点随机数 [0.0, 1.0)
     * @return 0.0 到 1.0 之间的随机浮点数
     */
    float NextFloat();

    /**
     * @brief 生成一个范围内的浮点随机数
     * @param min 最小值
     * @param max 最大值
     * @return min 到 max 之间的随机浮点数
     */
    float NextFloat(float min, float max);

    /**
     * @brief 生成一个布尔随机值
     * @return true 或 false（50%概率）
     */
    bool NextBool();

    /**
     * @brief 重置随机数生成器
     * @param seed 新种子
     */
    void Seed(uint64_t seed);

    /**
     * @brief 获取当前状态（用于序列化）
     * @return 状态数组 [state_, sequence_, initFlag_]
     */
    struct State {
        uint64_t state_;
        uint64_t sequence_;
        uint32_t initFlag_;
    };
    
    State GetState() const;

    /**
     * @brief 恢复状态（用于反序列化）
     */
    void SetState(const State& state);

private:
    uint64_t state_;          ///< 内部状态
    uint64_t sequence_;       ///< 序列常量
    uint32_t initFlag_;       ///< 初始化标志
    
    /**
     * @brief PCG 内部步进步骤
     */
    void Advance();
    
    /**
     * @brief 从内部状态生成随机数
     */
    uint32_t GenerateRaw();
};

// ================================================================
// 工具函数
// ================================================================

/**
 * @brief 从当前时间派生种子（不保证确定性）
 * @return 派生的种子值
 * 
 * @note 如果需要确定性，应使用固定种子
 */
uint64_t DeriveSeedFromTime();

/**
 * @brief 验证随机数生成器的确定性
 * @return 如果确定性验证通过返回true
 * 
 * 此函数生成两个相同种子的随机数序列并比较结果。
 * 用于测试和调试。
 */
bool VerifyDeterminism();

} // namespace PhysicsSync
