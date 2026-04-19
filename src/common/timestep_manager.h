/**
 * @file timestep_manager.h
 * @brief 确定性时间步长管理器
 * 
 * 本模块负责维护服务器和客户端的物理模拟时间步长，确保所有客户端
 * 在相同输入下产生逐帧一致的物理模拟结果。
 * 
 * 核心原理：
 * 1. 固定时间步长（Fixed Timestep）- 物理引擎以固定频率更新
 * 2. 插值渲染 - 渲染帧率可以高于物理更新率
 * 3. 累加器模式 - 累积 deltaTime，当达到固定步长时推进物理模拟
 */

#pragma once

#include <cstdint>
#include <chrono>
#include <cassert>
#include <stdexcept>
#include <string>
#include <functional>

namespace PhysicsSync {

// ================================================================
// 常量定义
// ================================================================

/** @brief 默认物理更新频率（赫兹） */
constexpr uint32_t DEFAULT_PHYSICS_HZ = 60;

/** @brief 默认物理时间步长（秒） */
constexpr float DEFAULT_FIXED_TIMESTEP = 1.0f / DEFAULT_PHYSICS_HZ;

/** @brief 最大允许子步数 - 防止帧率过低时物理模拟堆积 */
constexpr int MAX_SUBSTEPS = 8;

/** @brief 最小时间步长（防止除以零） */
constexpr float MIN_TIMESTEP = 1.0f / 240.0f;

// ================================================================
// 时间步长管理器
// ================================================================

/**
 * @brief 确定性时间步长管理器
 * 
 * 该管理器维护一个精确的固定时间步长循环，用于物理模拟。
 * 它是帧同步架构的核心组件，确保物理模拟的确定性。
 * 
 * 使用示例：
 * @code
 *     TimeStepManager manager;
 *     manager.SetFixedTimeStep(1.0f / 60.0f);
 *     
 *     while (running) {
 *         float deltaTime = GetRealTimeDelta();
 *         manager.StartNewFrame(deltaTime);
 *         
 *         while (manager.ShouldTick()) {
 *             PhysicsTick();  // 执行物理模拟
 *             manager.Tick();
 *         }
 *         
 *         manager.FinishFrame();
 *         Render(manager.GetInterpolationAlpha());
 *     }
 * @endcode
 */
class TimeStepManager {
public:
    /**
     * @brief 构造函数
     */
    TimeStepManager();

    /**
     * @brief 析构函数
     */
    ~TimeStepManager() = default;

    /**
     * @brief 设置固定时间步长
     * @param hertz 物理更新频率（赫兹），例如 60 表示每秒60次更新
     * @throw std::invalid_argument 如果频率无效
     */
    void SetFixedTimeStep(float hertz);

    /**
     * @brief 获取固定时间步长
     * @return 时间步长（秒）
     */
    float GetFixedTimeStep() const { return fixedTimeStep_; }

    /**
     * @brief 获取物理更新频率
     * @return 频率（赫兹）
     */
    uint32_t GetPhysicsHz() const { return physicsHz_; }

    /**
     * @brief 开始新的渲染帧
     * @param realDeltaTime 真实世界的时间增量（秒）
     * 
     * 必须在每帧开始时调用，传入从上一帧到现在的真实时间增量。
     */
    void StartNewFrame(float realDeltaTime);

    /**
     * @brief 检查是否应该进行物理tick
     * @return 如果应该执行物理tick则返回true
     * 
     * 当累积的时间达到固定时间步长时返回true。
     */
    bool ShouldTick() const { return accumulator_ >= fixedTimeStep_; }

    /**
     * @brief 执行一次物理tick
     * @return 当前tick序号
     * 
     * 推进时间步长并返回当前tick序号。
     * 应在ShouldTick()返回true时调用。
     */
    uint32_t Tick();

    /**
     * @brief 结束当前帧
     * 
     * 必须在每帧结束时调用，用于重置插值alpha等状态。
     */
    void FinishFrame();

    /**
     * @brief 获取插值alpha值
     * @return 介于0.0到1.0之间的值，用于渲染插值
     * 
     * 当用于预测性渲染时，此值表示当前渲染帧在两个物理tick之间的位置。
     * 0.0表示刚好在新tick之后，1.0表示即将到达下一个tick。
     */
    float GetInterpolationAlpha() const;

    /**
     * @brief 获取当前物理tick序号
     * @return 从启动开始累计的tick数
     */
    uint32_t GetCurrentTick() const { return currentTick_; }

    /**
     * @brief 获取累积时间
     * @return 未处理的累积时间（秒）
     */
    float GetAccumulator() const { return accumulator_; }

    /**
     * @brief 重置时间步长管理器
     * 
     * 将管理器恢复到初始状态。
     * 适用于场景切换或重新连接等情况。
     */
    void Reset();

    /**
     * @brief 强制设置tick序号
     * 
     * 用于客户端接收到服务器快照后同步tick序号。
     * @param tick 目标tick序号
     */
    void ForceSetTick(uint32_t tick);

    /**
     * @brief 获取运行统计信息
     * @return 包含统计信息的字符串
     */
    std::string GetStatistics() const;

    /**
     * @brief 获取错过的tick数量
     * @return 当前帧错过的tick数
     */
    int GetMissedTicks() const { return missedTicks_; }

private:
    float fixedTimeStep_;           ///< 固定时间步长（秒）
    uint32_t physicsHz_;            ///< 物理更新频率（赫兹）
    
    float accumulator_;             ///< 时间累加器
    uint32_t currentTick_;          ///< 当前tick序号
    
    int missedTicks_;               ///< 当前帧错过的tick数
    bool frameStarted_;             ///< 当前帧是否已开始

    // 统计信息
    uint32_t totalTicks_;           ///< 总tick数
    float lastFrameTime_;           ///< 上一帧耗时
};

// ================================================================
// 辅助函数
// ================================================================

/**
 * @brief 将赫兹转换为秒
 * @param hertz 频率（赫兹）
 * @return 时间间隔（秒）
 */
inline float HertzToSeconds(float hertz) {
    if (hertz <= 0.0f) {
        throw std::invalid_argument("Hertz must be positive");
    }
    return 1.0f / hertz;
}

/**
 * @brief 将秒转换为赫兹
 * @param seconds 时间间隔（秒）
 * @return 频率（赫兹）
 */
inline float SecondsToHertz(float seconds) {
    if (seconds <= 0.0f) {
        throw std::invalid_argument("Seconds must be positive");
    }
    return 1.0f / seconds;
}

/**
 * @brief 获取高精度时钟
 * @return 自某个固定点以来的纳秒数
 */
uint64_t GetHighPrecisionClock();

/**
 * @brief 将纳秒转换为秒
 * @param nanoseconds 纳秒数
 * @return 秒数
 */
inline double NanosecondsToSeconds(uint64_t nanoseconds) {
    return static_cast<double>(nanoseconds) / 1e9;
}

} // namespace PhysicsSync
