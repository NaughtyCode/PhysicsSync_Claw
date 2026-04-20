/**
 * @file physics_state.cpp
 * @brief 物理状态相关实现文件
 * 
 * 包含物理状态结构的辅助实现，如状态比较、校验和计算等。
 */

#include "physics_state.h"
#include <cstring>

namespace PhysicsSync {

// =====================================================================
// PhysicsObjectState 实现
// =====================================================================

/**
 * @brief 计算对象状态的校验和
 * @return 简单哈希值，用于快速比较两个状态是否一致
 * @note 这不是加密安全的哈希，仅用于调试和验证
 */
uint32_t PhysicsObjectState::ComputeChecksum() const {
    // 简单的累加哈希
    uint32_t hash = 0;
    
    // 将内存内容按字节哈希
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(this);
    // 不包括sequenceNumber和flags
    size_t hashSize = offsetof(PhysicsObjectState, sequenceNumber);
    
    for (size_t i = 0; i < hashSize; i++) {
        hash = hash * 31 + bytes[i];
    }
    
    return hash;
}

/**
 * @brief 比较两个对象状态是否相等（在容差范围内）
 * @param other 要比较的状态
 * @param positionTolerance 位置容差（米）
 * @param rotationTolerance 旋转容差（弧度）
 * @param velocityTolerance 速度容差（米/秒）
 * @return 如果两个状态基本相等则返回true
 */
bool PhysicsObjectState::IsApproximatelyEqual(
    const PhysicsObjectState& other,
    float positionTolerance,
    float rotationTolerance,
    float velocityTolerance) const {
    
    // 比较位置
    Vec3 posDiff = position - other.position;
    if (posDiff.Length() > positionTolerance) {
        return false;
    }
    
    // 比较旋转（使用四元数点积）
    float rotationDot = std::abs(
        rotation.x * other.rotation.x +
        rotation.y * other.rotation.y +
        rotation.z * other.rotation.z +
        rotation.w * other.rotation.w);
    // 旋转差异：当点积接近1时，旋转几乎相同
    if ((1.0f - rotationDot) > rotationTolerance) {
        return false;
    }
    
    // 比较线速度
    Vec3 velDiff = linearVelocity - other.linearVelocity;
    if (velDiff.Length() > velocityTolerance) {
        return false;
    }
    
    // 比较角速度
    Vec3 angVelDiff = angularVelocity - other.angularVelocity;
    if (angVelDiff.Length() > velocityTolerance) {
        return false;
    }
    
    return true;
}

// =====================================================================
// Vec3 实现
// =====================================================================

/**
 * @brief 计算向量与另一个向量之间的欧几里得距离
 */
float Vec3::DistanceTo(const Vec3& other) const {
    return (*this - other).Length();
}

/**
 * @brief 对向量进行截断，限制其长度不超过最大值
 */
Vec3 Vec3::ClampLength(float maxLength) const {
    float len = Length();
    if (len > maxLength && len > 1e-8f) {
        return (*this) * (maxLength / len);
    }
    return *this;
}

// =====================================================================
// Quat 实现
// =====================================================================

/**
 * @brief 将四元数转换为轴角表示
 * @param axis 输出的旋转轴
 * @param angle 输出的旋转角度（弧度）
 */
void Quat::ToAxisAngle(Vec3& axis, float& angle) const {
    // 确保四元数是归一化的
    float len = std::sqrt(x*x + y*y + z*z + w*w);
    if (len < 1e-8f) {
        axis = Vec3::Up();
        angle = 0.0f;
        return;
    }
    
    float norm = 1.0f / len;
    float nx = x * norm;
    float ny = y * norm;
    float nz = z * norm;
    float nw = w * norm;
    
    // 检查是否为无效四元数
    if (nw > 1.0f) nw = 1.0f;
    if (nw < -1.0f) nw = -1.0f;
    
    angle = 2.0f * std::acos(nw);
    
    float sinAngle = std::sin(angle * 0.5f);
    if (sinAngle > 1e-8f) {
        axis.x = nx / sinAngle;
        axis.y = ny / sinAngle;
        axis.z = nz / sinAngle;
    } else {
        axis = Vec3::Up();
    }
}

/**
 * @brief 从轴角表示构造四元数
 * @param axis 旋转轴（需要归一化）
 * @param angle 旋转角度（弧度）
 * @return 对应的四元数
 */
Quat Quat::FromAxisAngle(const Vec3& axis, float angle) {
    Quat result;
    float halfAngle = angle * 0.5f;
    float sinHalf = std::sin(halfAngle);
    
    Vec3 normalizedAxis = axis.Normalized();
    result.x = normalizedAxis.x * sinHalf;
    result.y = normalizedAxis.y * sinHalf;
    result.z = normalizedAxis.z * sinHalf;
    result.w = std::cos(halfAngle);
    
    return result;
}

/**
 * @brief 计算两个四元数之间的差异
 * @param other 目标四元数
 * @return 差异值（0表示相同，越大表示差异越大）
 */
float Quat::Difference(const Quat& other) const {
    float dot = std::abs(
        this->x * other.x +
        this->y * other.y +
        this->z * other.z +
        this->w * other.w);
    return 1.0f - dot;
}

// =====================================================================
// PhysicsWorldSnapshot 实现
// =====================================================================

/**
 * @brief 查找指定ID的对象状态
 * @param objectId 对象ID
 * @return 指向对象状态的指针，如果未找到则返回nullptr
 */
PhysicsObjectState* PhysicsWorldSnapshot::FindObject(uint32_t objectId) {
    for (auto& obj : objects) {
        if (obj.objectId == objectId) {
            return &obj;
        }
    }
    return nullptr;
}

const PhysicsObjectState* PhysicsWorldSnapshot::FindObject(uint32_t objectId) const {
    for (const auto& obj : objects) {
        if (obj.objectId == objectId) {
            return &obj;
        }
    }
    return nullptr;
}

bool PhysicsWorldSnapshot::RemoveObject(uint32_t objectId) {
    auto it = std::remove_if(objects.begin(), objects.end(),
        [objectId](const PhysicsObjectState& obj) {
            return obj.objectId == objectId;
        });
    
    bool removed = (it != objects.end());
    objects.erase(it, objects.end());
    objectCount = static_cast<uint32_t>(objects.size());
    
    return removed;
}

bool PhysicsWorldSnapshot::Deserialize(const uint8_t*& data) {
    std::memcpy(&snapshotId, data, sizeof(uint32_t));
    data += sizeof(uint32_t);
    std::memcpy(&timestamp, data, sizeof(uint32_t));
    data += sizeof(uint32_t);
    std::memcpy(&objectCount, data, sizeof(uint32_t));
    data += sizeof(uint32_t);
    
    objects.resize(objectCount);
    for (auto& obj : objects) {
        if (!obj.Deserialize(data)) {
            return false;
        }
    }
    return true;
}

} // namespace PhysicsSync
