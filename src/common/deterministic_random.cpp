/**
 * @file deterministic_random.cpp
 * @brief 确定性随机数生成器实现
 * 
 * 实现PCG32算法的确定性版本，确保跨平台一致性。
 */

#include "deterministic_random.h"
#include <ctime>
#include <cstring>

namespace PhysicsSync {

// =====================================================================
// PCG32 实现
// =====================================================================

// PCG32 常量（确定性）
static const uint64_t kInc0 = 1013904223ULL;
static const uint64_t kInc1 = 0x14057B7EFFULL;
static const uint64_t kMult = 6364136223846793005ULL;

DeterministicRandom::DeterministicRandom()
    : DeterministicRandom(42ULL, 0)
{
}

DeterministicRandom::DeterministicRandom(uint64_t seed, uint64_t stream)
    : state_(0)
    , sequence_(stream * 2 + 1)
    , initFlag_(0)
{
    // 初始化PCG状态（与平台无关）
    state_ = 0;
    Advance();
    state_ = state_ + seed;
    Advance();
    initFlag_ = 0xC70F6907U;  // 魔术数字表示已初始化
}

uint32_t DeterministicRandom::Next() {
    if (initFlag_ != 0xC70F6907U) {
        throw std::runtime_error("DeterministicRandom not initialized");
    }
    return GenerateRaw();
}

uint32_t DeterministicRandom::Next(uint32_t min, uint32_t max) {
    if (min > max) {
        return min;
    }
    uint32_t range = max - min + 1;
    if (range == 0) {
        return min;
    }
    return min + (Next() % range);
}

float DeterministicRandom::NextFloat() {
    return static_cast<float>(Next()) / static_cast<float>(UINT32_MAX);
}

float DeterministicRandom::NextFloat(float min, float max) {
    return min + NextFloat() * (max - min);
}

bool DeterministicRandom::NextBool() {
    return (Next() & 1) != 0;
}

void DeterministicRandom::Seed(uint64_t seed) {
    state_ = 0;
    Advance();
    state_ = state_ + seed;
    Advance();
    initFlag_ = 0xC70F6907U;
}

DeterministicRandom::State DeterministicRandom::GetState() const {
    return {state_, sequence_, initFlag_};
}

void DeterministicRandom::SetState(const State& state) {
    state_ = state.state_;
    sequence_ = state.sequence_;
    initFlag_ = state.initFlag_;
}

void DeterministicRandom::Advance() {
    state_ = state_ * kMult + sequence_;
}

uint32_t DeterministicRandom::GenerateRaw() {
    uint32_t oldState = static_cast<uint32_t>(state_);
    
    // XSH-RR 输出函数
    uint32_t xorShifted = (((oldState >> 18u) ^ oldState) >> 27u);
    uint32_t rightShift = oldState >> 59u;
    
    Advance();
    
    return (xorShifted >> rightShift) | (xorShifted << ((-rightShift) & 31));
}

// =====================================================================
// 工具函数实现
// =====================================================================

uint64_t DeriveSeedFromTime() {
    auto now = std::time(nullptr);
    uint64_t seed = static_cast<uint64_t>(now);
    // 添加一些混淆
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return seed;
}

bool VerifyDeterminism() {
    DeterministicRandom rng1(12345);
    DeterministicRandom rng2(12345);
    
    for (int i = 0; i < 100; i++) {
        if (rng1.Next() != rng2.Next()) {
            return false;
        }
    }
    
    return true;
}

} // namespace PhysicsSync
