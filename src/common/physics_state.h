/**
 * @file physics_state.h
 * @brief 物理状态定义 - 用于服务器权威物理同步的核心数据结构
 * 
 * 本文件定义了物理世界中刚体的状态信息，包括位置、旋转、速度等。
 * 这些状态信息会在服务器和客户端之间传输，用于同步物理世界。
 * 
 * 设计原则：
 * 1. 数据紧凑 - 减少网络传输带宽
 * 2. 确定性问题 - 使用固定精度浮点数
 * 3. 可扩展性 - 支持不同种类的物理对象
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace PhysicsSync {

// ====================================================================
// 基础数学类型定义
// ====================================================================

/**
 * @brief 三维向量 - 用于位置、速度、力等
 * 
 * 注意：为了确保跨平台一致性，所有浮点运算应在确定性环境下进行。
 * 服务器端和客户端必须使用相同的浮点模型编译。
 */
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    /** @brief 零向量 */
    static Vec3 Zero() { return Vec3{0.0f, 0.0f, 0.0f}; }

    /** @brief 单位向量（向上） */
    static Vec3 Up() { return Vec3{0.0f, 1.0f, 0.0f}; }

    /** @brief 向量加法 */
    Vec3 operator+(const Vec3& other) const {
        return Vec3{x + other.x, y + other.y, z + other.z};
    }

    /** @brief 向量减法 */
    Vec3 operator-(const Vec3& other) const {
        return Vec3{x - other.x, y - other.y, z - other.z};
    }

    /** @brief 向量标量乘法 */
    Vec3 operator*(float scalar) const {
        return Vec3{x * scalar, y * scalar, z * scalar};
    }

    /** @brief 向量点乘 */
    float Dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    /** @brief 向量叉乘 */
    Vec3 Cross(const Vec3& other) const {
        return Vec3{
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }

    /** @brief 向量模长 */
    float Length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    /** @brief 向量归一化 */
    Vec3 Normalized() const {
        float len = Length();
        if (len > 1e-8f) {
            return Vec3{x / len, y / len, z / len};
        }
        return Vec3::Zero();
    }

    /** @brief 序列化到字节数组 */
    void Serialize(std::vector<uint8_t>& buffer) const {
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&x), 
            reinterpret_cast<const uint8_t*>(&x) + sizeof(float));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&y), 
            reinterpret_cast<const uint8_t*>(&y) + sizeof(float));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&z), 
            reinterpret_cast<const uint8_t*>(&z) + sizeof(float));
    }

    /** @brief 从字节数组反序列化 */
    void Deserialize(const uint8_t*& data) {
        std::memcpy(&x, data, sizeof(float));
        data += sizeof(float);
        std::memcpy(&y, data, sizeof(float));
        data += sizeof(float);
        std::memcpy(&z, data, sizeof(float));
        data += sizeof(float);
    }
    
    /** @brief 序列化后的大小（字节） */
    static constexpr size_t SerializedSizeBytes() {
        return sizeof(float) * 3;
    }
};

/**
 * @brief 四元数 - 用于表示旋转（避免万向锁问题）
 * 
 * 四元数比欧拉角更适合网络同步，因为它可以平滑插值（slerp）。
 */
struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    /** @brief 单位四元数（无旋转） */
    static Quat Identity() { return Quat{0.0f, 0.0f, 0.0f, 1.0f}; }

    /** @brief 球面线性插值 - 用于平滑过渡 */
    Quat Slerp(const Quat& target, float t) const {
        // 确保插值走最短路径
        float dot = this->x * target.x + this->y * target.y + 
                    this->z * target.z + this->w * target.w;
        
        Quat t2 = target;
        if (dot < 0.0f) {
            t2 = Quat{-target.x, -target.y, -target.z, -target.w};
            dot = -dot;
        }

        // 防止除零，当四元数非常接近时，使用线性插值
        if (dot > 0.9995f) {
            return Quat{
                this->x + (t2.x - this->x) * t,
                this->y + (t2.y - this->y) * t,
                this->z + (t2.z - this->z) * t,
                this->w + (t2.w - this->w) * t
            };
        }

        // 标准球面线性插值
        float theta = std::acos(dot);
        float sinTheta = std::sin(theta);
        
        float s1 = std::sin((1.0f - t) * theta) / sinTheta;
        float s2 = std::sin(t * theta) / sinTheta;

        return Quat{
            s1 * this->x + s2 * t2.x,
            s1 * this->y + s2 * t2.y,
            s1 * this->z + s2 * t2.z,
            s1 * this->w + s2 * t2.w
        };
    }

    /** @brief 序列化到字节数组 */
    void Serialize(std::vector<uint8_t>& buffer) const {
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&x), 
            reinterpret_cast<const uint8_t*>(&x) + sizeof(float));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&y), 
            reinterpret_cast<const uint8_t*>(&y) + sizeof(float));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&z), 
            reinterpret_cast<const uint8_t*>(&z) + sizeof(float));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&w), 
            reinterpret_cast<const uint8_t*>(&w) + sizeof(float));
    }

    /** @brief 从字节数组反序列化 */
    void Deserialize(const uint8_t*& data) {
        std::memcpy(&x, data, sizeof(float));
        data += sizeof(float);
        std::memcpy(&y, data, sizeof(float));
        data += sizeof(float);
        std::memcpy(&z, data, sizeof(float));
        data += sizeof(float);
        std::memcpy(&w, data, sizeof(float));
        data += sizeof(float);
    }

    /** @brief 序列化后的大小（字节） */
    static constexpr size_t SerializedSizeBytes() {
        return sizeof(float) * 4;
    }
};

// ====================================================================
// 物理对象类型
// ====================================================================

/**
 * @brief 物理对象类型枚举
 * 
 * 不同类型的物理对象有不同的同步策略：
 * - PLAYER: 玩家控制的物体，高频同步
 * - DYNAMIC: 动态物体，中频同步
 * - KINEMATIC: 运动学物体，低频同步
 * - STATIC: 静态物体，不需要同步
 */
enum class PhysicsObjectType : uint8_t {
    PLAYER = 0,     ///< 玩家控制的物体
    DYNAMIC = 1,    ///< 动态物理物体
    KINEMATIC = 2,  ///< 运动学物体
    STATIC = 3      ///< 静态物体
};

// ====================================================================
// 物理对象状态
// ====================================================================

/**
 * @brief 物理对象状态 - 同步的核心数据结构
 * 
 * 包含一个刚体在特定时刻的完整状态信息。
 * 服务器定期发送此结构体给所有客户端。
 * 
 * @note 为了减少带宽，只存储必要的状态信息
 *       位置(12字节) + 旋转(16字节) + 线速度(12字节) + 角速度(12字节) = 52字节
 */
struct PhysicsObjectState {
    // 标识符
    uint32_t objectId = 0;          ///< 对象唯一ID
    PhysicsObjectType type = PhysicsObjectType::DYNAMIC; ///< 对象类型
    
    // 变换状态
    Vec3 position;                  ///< 当前位置
    Quat rotation;                  ///< 当前旋转（四元数）
    
    // 速度状态（用于客户端预测和插值）
    Vec3 linearVelocity;            ///< 线速度 (单位: m/s)
    Vec3 angularVelocity;           ///< 角速度 (单位: rad/s)
    
    // 时间戳和标志
    uint32_t sequenceNumber = 0;    ///< 序列号（用于检测丢包和乱序）
    uint32_t flags = 0;             ///< 状态标志
    
    // 默认构造函数
    PhysicsObjectState() = default;

    /**
     * @brief 构造函数 - 初始化一个新的物理对象状态
     * @param id 对象ID
     * @param pos 初始位置
     * @param rot 初始旋转
     * @param objType 对象类型
     */
    PhysicsObjectState(uint32_t id, Vec3 pos, Quat rot, PhysicsObjectType objType)
        : objectId(id), type(objType), position(pos), rotation(rot),
          linearVelocity(Vec3::Zero()), angularVelocity(Vec3::Zero()),
          sequenceNumber(0), flags(0) {}

    /**
     * @brief 预测下一帧的位置（客户端使用）
     * @param deltaTime 时间增量（秒）
     * @return 预测的未来位置
     */
    Vec3 PredictPosition(float deltaTime) const {
        return position + linearVelocity * deltaTime;
    }

    /**
     * @brief 预测下一帧的旋转（客户端使用）
     * @param deltaTime 时间增量（秒）
     * @return 预测的未来旋转
     */
    Quat PredictRotation(float deltaTime) const {
        // 角速度转换为四元数旋转
        float angle = angularVelocity.Length() * deltaTime;
        Vec3 axis = angularVelocity.Normalized();
        
        // 从轴角构造四元数
        float halfAngle = angle * 0.5f;
        float sinHalf = std::sin(halfAngle);
        
        Quat deltaRot{
            axis.x * sinHalf,
            axis.y * sinHalf,
            axis.z * sinHalf,
            std::cos(halfAngle)
        };
        
        return rotation.Slerp(deltaRot, 1.0f);
    }

    /** @brief 序列化 - 将状态转换为字节流 */
    void Serialize(std::vector<uint8_t>& buffer) const {
        // 序列化标识符
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&objectId), 
            reinterpret_cast<const uint8_t*>(&objectId) + sizeof(uint32_t));
        
        buffer.push_back(static_cast<uint8_t>(type));
        
        // 序列化位置、旋转和速度
        position.Serialize(buffer);
        rotation.Serialize(buffer);
        linearVelocity.Serialize(buffer);
        angularVelocity.Serialize(buffer);
        
        // 序列化元数据
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&sequenceNumber), 
            reinterpret_cast<const uint8_t*>(&sequenceNumber) + sizeof(uint32_t));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&flags), 
            reinterpret_cast<const uint8_t*>(&flags) + sizeof(uint32_t));
    }

    /**
     * @brief 反序列化 - 从字节流恢复状态
     * @param data 指向字节流的指针（会自动前进）
     * @return 是否成功反序列化
     */
    bool Deserialize(const uint8_t*& data) {
        try {
            // 反序列化标识符
            std::memcpy(&objectId, data, sizeof(uint32_t));
            data += sizeof(uint32_t);
            
            type = static_cast<PhysicsObjectType>(*data);
            data += sizeof(uint8_t);
            
            // 反序列化位置、旋转和速度
            position.Deserialize(data);
            rotation.Deserialize(data);
            linearVelocity.Deserialize(data);
            angularVelocity.Deserialize(data);
            
            // 反序列化元数据
            std::memcpy(&sequenceNumber, data, sizeof(uint32_t));
            data += sizeof(uint32_t);
            std::memcpy(&flags, data, sizeof(uint32_t));
            data += sizeof(uint32_t);
            
            return true;
        } catch (...) {
            return false;
        }
    }

    /** @brief 计算序列化后的字节大小 */
    static constexpr size_t SerializedSizeBytes() {
        return sizeof(uint32_t) + sizeof(uint8_t) + 
               Vec3::SerializedSizeBytes() * 4 + 
               Quat::SerializedSizeBytes() + 
               sizeof(uint32_t) + sizeof(uint32_t);
    }

    /** @brief 获取序列化后的字节大小 */
    size_t Size() const {
        return SerializedSizeBytes();
    }

    /** @brief 计算对象状态的校验和 */
    uint32_t ComputeChecksum() const;

    /**
     * @brief 比较两个对象状态是否相等（在容差范围内）
     * @param other 要比较的状态
     * @param positionTolerance 位置容差（米）
     * @param rotationTolerance 旋转容差（弧度）
     * @param velocityTolerance 速度容差（米/秒）
     * @return 如果两个状态基本相等则返回true
     */
    bool IsApproximatelyEqual(
        const PhysicsObjectState& other,
        float positionTolerance = 0.01f,
        float rotationTolerance = 0.01f,
        float velocityTolerance = 0.1f) const;
};

// ====================================================================
// 物理世界快照
// ====================================================================

/**
 * @brief 物理世界快照 - 包含多个物理对象的状态
 * 
 * 用于：
 * 1. 服务器定期广播世界状态
 * 2. 客户端进行状态校正
 * 3. 服务器回放和调试
 */
struct PhysicsWorldSnapshot {
    uint32_t snapshotId = 0;            ///< 快照唯一ID
    uint32_t timestamp = 0;             ///< 快照时间戳（毫秒）
    uint32_t objectCount = 0;           ///< 对象数量
    
    std::vector<PhysicsObjectState> objects;  ///< 对象状态列表
    
    /** @brief 构造函数 */
    PhysicsWorldSnapshot() = default;

    /**
     * @brief 添加一个对象状态
     * @param state 对象状态
     */
    void AddObject(const PhysicsObjectState& state) {
        objects.push_back(state);
        objectCount = static_cast<uint32_t>(objects.size());
    }

    /** @brief 清空所有对象 */
    void Clear() {
        objects.clear();
        objectCount = 0;
        snapshotId++;
    }

    /** @brief 序列化 */
    void Serialize(std::vector<uint8_t>& buffer) const {
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&snapshotId), 
            reinterpret_cast<const uint8_t*>(&snapshotId) + sizeof(uint32_t));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&timestamp), 
            reinterpret_cast<const uint8_t*>(&timestamp) + sizeof(uint32_t));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&objectCount), 
            reinterpret_cast<const uint8_t*>(&objectCount) + sizeof(uint32_t));
        
        for (const auto& obj : objects) {
            obj.Serialize(buffer);
        }
    }

    /** @brief 反序列化 */
    bool Deserialize(const uint8_t*& data) {
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
};

// ====================================================================
// 玩家输入
// ====================================================================

/**
 * @brief 玩家输入结构体
 * 
 * 客户端将玩家的输入发送给服务器，服务器根据输入更新物理世界。
 * 输入包括移动方向、跳跃、攻击等动作。
 */
struct PlayerInput {
    uint32_t playerId = 0;              ///< 玩家ID
    uint32_t inputHash = 0;             ///< 输入校验和（用于检测异常输入）
    uint32_t inputTick = 0;             ///< 输入所属的tick
    
    // 输入数据
    float moveX = 0.0f;                 ///< 移动X轴（-1.0 ~ 1.0）
    float moveY = 0.0f;                 ///< 移动Y轴（-1.0 ~ 1.0）
    float lookX = 0.0f;                 ///< 视角水平旋转
    float lookY = 0.0f;                 ///< 视角垂直旋转
    
    // 按钮状态
    uint32_t buttons = 0;               ///< 按钮位掩码
    
    /** @brief 默认构造函数 */
    PlayerInput() = default;

    /**
     * @brief 构造函数
     * @param id 玩家ID
     * @param tick 输入tick
     */
    explicit PlayerInput(uint32_t id, uint32_t tick)
        : playerId(id), inputTick(tick) {}

    /** @brief 计算输入校验和 */
    void ComputeHash() {
        // 简单哈希 - 实际项目应使用更健壮的哈希算法
        inputHash = 0;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
        for (size_t i = 0; i < sizeof(PlayerInput) - sizeof(uint32_t); i++) {
            inputHash = inputHash * 31 + data[i];
        }
    }

    /** @brief 序列化 */
    void Serialize(std::vector<uint8_t>& buffer) const {
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&playerId), 
            reinterpret_cast<const uint8_t*>(&playerId) + sizeof(uint32_t));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&inputHash), 
            reinterpret_cast<const uint8_t*>(&inputHash) + sizeof(uint32_t));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&inputTick), 
            reinterpret_cast<const uint8_t*>(&inputTick) + sizeof(uint32_t));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&moveX), 
            reinterpret_cast<const uint8_t*>(&moveX) + sizeof(float));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&moveY), 
            reinterpret_cast<const uint8_t*>(&moveY) + sizeof(float));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&lookX), 
            reinterpret_cast<const uint8_t*>(&lookX) + sizeof(float));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&lookY), 
            reinterpret_cast<const uint8_t*>(&lookY) + sizeof(float));
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(&buttons), 
            reinterpret_cast<const uint8_t*>(&buttons) + sizeof(uint32_t));
    }

    /** @brief 反序列化 */
    bool Deserialize(const uint8_t*& data) {
        std::memcpy(&playerId, data, sizeof(uint32_t));
        data += sizeof(uint32_t);
        std::memcpy(&inputHash, data, sizeof(uint32_t));
        data += sizeof(uint32_t);
        std::memcpy(&inputTick, data, sizeof(uint32_t));
        data += sizeof(uint32_t);
        std::memcpy(&moveX, data, sizeof(float));
        data += sizeof(float);
        std::memcpy(&moveY, data, sizeof(float));
        data += sizeof(float);
        std::memcpy(&lookX, data, sizeof(float));
        data += sizeof(float);
        std::memcpy(&lookY, data, sizeof(float));
        data += sizeof(float);
        std::memcpy(&buttons, data, sizeof(uint32_t));
        data += sizeof(uint32_t);
        return true;
    }

    /** @brief 获取序列化后的字节大小 */
    static constexpr size_t SerializedSizeBytes() {
        return sizeof(uint32_t) * 4 + sizeof(float) * 4;
    }
};

} // namespace PhysicsSync
