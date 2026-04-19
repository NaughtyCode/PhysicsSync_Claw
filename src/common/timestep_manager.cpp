/**
 * @file timestep_manager.cpp
 * @brief 确定性时间步长管理器实现
 * 
 * 实现固定时间步长循环逻辑，支持插值渲染。
 */

#include "timestep_manager.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <chrono>
#endif

namespace PhysicsSync {

// ================================================================
// TimeStepManager 实现
// ================================================================

TimeStepManager::TimeStepManager()
    : fixedTimeStep_(DEFAULT_FIXED_TIMESTEP)
    , physicsHz_(DEFAULT_PHYSICS_HZ)
    , accumulator_(0.0f)
    , currentTick_(0)
    , missedTicks_(0)
    , frameStarted_(false)
    , totalTicks_(0)
    , lastFrameTime_(0.0f)
{
}

void TimeStepManager::SetFixedTimeStep(float hertz) {
    if (hertz <= 0.0f || hertz > 240.0f) {
        throw std::invalid_argument("Hertz must be between 0 and 240");
    }
    fixedTimeStep_ = HertzToSeconds(hertz);
    physicsHz_ = static_cast<uint32_t>(hertz);
}

void TimeStepManager::StartNewFrame(float realDeltaTime) {
    // 防止过大deltaTime（例如切换窗口时）
    realDeltaTime = std::min(realDeltaTime, 0.25f);
    
    accumulator_ += realDeltaTime;
    missedTicks_ = 0;
    frameStarted_ = true;
}

uint32_t TimeStepManager::Tick() {
    accumulator_ -= fixedTimeStep_;
    currentTick_++;
    totalTicks_++;
    return currentTick_;
}

void TimeStepManager::FinishFrame() {
    // 检查是否落后太多
    if (accumulator_ > fixedTimeStep_ * MAX_SUBSTEPS) {
        // 重置累加器，避免灾难性累积
        accumulator_ = 0.0f;
        missedTicks_ = 0;
    }
    
    frameStarted_ = false;
}

float TimeStepManager::GetInterpolationAlpha() const {
    if (accumulator_ < 0.0f) return 0.0f;
    return accumulator_ / fixedTimeStep_;
}

void TimeStepManager::Reset() {
    accumulator_ = 0.0f;
    currentTick_ = 0;
    missedTicks_ = 0;
    totalTicks_ = 0;
    frameStarted_ = false;
}

void TimeStepManager::ForceSetTick(uint32_t tick) {
    currentTick_ = tick;
    accumulator_ = 0.0f;
}

std::string TimeStepManager::GetStatistics() const {
    std::ostringstream oss;
    oss << "TimeStep Stats:" << std::endl
        << "  Tick: " << currentTick_ << std::endl
        << "  Total: " << totalTicks_ << std::endl
        << "  Step: " << std::fixed << std::setprecision(4) 
        << fixedTimeStep_ << "s (" << physicsHz_ << "Hz)" << std::endl
        << "  Accumulator: " << accumulator_ << "s" << std::endl
        << "  Missed: " << missedTicks_ << std::endl
        << "  Alpha: " << GetInterpolationAlpha() << std::endl;
    return oss.str();
}

// ================================================================
// 高精度时钟实现
// ================================================================

uint64_t GetHighPrecisionClock() {
#ifdef _WIN32
    static LARGE_INTEGER freq{};
    static bool initialized = false;
    
    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = true;
    }
    
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>(counter.QuadPart);
#else
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
#endif
}

} // namespace PhysicsSync
