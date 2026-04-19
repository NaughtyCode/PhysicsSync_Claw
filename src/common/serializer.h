/**
 * @file serializer.h
 * @brief 二进制序列化/反序列化工具类
 * 
 * 本模块提供高效的二进制序列化功能，用于网络传输和状态保存。
 * 支持基本类型、容器类型和自定义类型的序列化。
 * 
 * 设计原则：
 * 1. 网络字节序 - 所有多字节整数使用大端序传输
 * 2. 类型安全 - 使用静态类型检查
 * 3. 高效性 - 直接内存拷贝，避免不必要的复制
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
#include <type_traits>

namespace PhysicsSync {

// ================================================================
// 序列化器
// ================================================================

/**
 * @brief 二进制序列化器
 * 
 * 将数据序列化为字节流，用于网络传输或持久化存储。
 * 使用网络字节序（大端序）以确保跨平台兼容性。
 */
class Serializer {
public:
    /**
     * @brief 构造函数
     * @param bufferSize 预分配的缓冲区大小
     */
    explicit Serializer(size_t bufferSize = 1024);

    /**
     * @brief 析构函数
     */
    ~Serializer() = default;

    // 禁止拷贝，允许移动
    Serializer(const Serializer&) = delete;
    Serializer& operator=(const Serializer&) = delete;
    Serializer(Serializer&&) noexcept = default;
    Serializer& operator=(Serializer&&) noexcept = default;

    /**
     * @brief 序列化一个字节
     * @param value 要序列化的值
     * @return 是否成功
     */
    bool Serialize(uint8_t value);

    /**
     * @brief 序列化一个16位整数
     * @param value 要序列化的值
     * @return 是否成功
     */
    bool Serialize(uint16_t value);

    /**
     * @brief 序列化一个32位整数
     * @param value 要序列化的值
     * @return 是否成功
     */
    bool Serialize(uint32_t value);

    /**
     * @brief 序列化一个64位整数
     * @param value 要序列化的值
     * @return 是否成功
     */
    bool Serialize(uint64_t value);

    /**
     * @brief 序列化一个32位浮点数
     * @param value 要序列化的值
     * @return 是否成功
     */
    bool Serialize(float value);

    /**
     * @brief 序列化一个64位浮点数
     * @param value 要序列化的值
     * @return 是否成功
     */
    bool Serialize(double value);

    /**
     * @brief 序列化一个布尔值
     * @param value 要序列化的值
     * @return 是否成功
     */
    bool Serialize(bool value);

    /**
     * @brief 序列化一个字符串
     * @param str 要序列化的字符串
     * @return 是否成功
     */
    bool Serialize(const std::string& str);

    /**
     * @brief 序列化原始数据块
     * @param data 数据指针
     * @param size 数据大小（字节）
     * @return 是否成功
     */
    bool SerializeRaw(const void* data, size_t size);

    /**
     * @brief 序列化一个容器
     * @param container 要序列化的容器
     * @return 是否成功
     */
    template<typename T>
    bool SerializeContainer(const std::vector<T>& container);

    /**
     * @brief 获取序列化后的数据
     * @return 字节向量（const引用）
     */
    const std::vector<uint8_t>& GetData() const { return buffer_; }

    /**
     * @brief 获取序列化后的数据大小
     * @return 字节数
     */
    size_t GetSize() const { return buffer_.size(); }

    /**
     * @brief 获取已序列化的字节数
     * @return 字节数
     */
    size_t GetBytesWritten() const { return bytesWritten_; }

    /**
     * @brief 重置序列化器
     */
    void Reset();

    /**
     * @brief 预留缓冲区空间
     * @param additionalSize 需要额外预留的空间
     */
    void Reserve(size_t additionalSize);

private:
    std::vector<uint8_t> buffer_;
    size_t bytesWritten_;

    /**
     * @brief 写入一个字节
     * @param byte 要写入的字节
     * @return 是否成功
     */
    bool WriteByte(uint8_t byte);

    /**
     * @brief 将主机字节序转换为网络字节序（16位）
     */
    static uint16_t HostToNetwork(uint16_t value);
    
    /**
     * @brief 将主机字节序转换为网络字节序（32位）
     */
    static uint32_t HostToNetwork(uint32_t value);
    
    /**
     * @brief 将主机字节序转换为网络字节序（64位）
     */
    static uint64_t HostToNetwork(uint64_t value);
};

// ================================================================
// 反序列化器
// ================================================================

/**
 * @brief 二进制反序列化器
 * 
 * 从字节流中反序列化数据。
 * 使用网络字节序（大端序）以确保跨平台兼容性。
 */
class Deserializer {
public:
    /**
     * @brief 构造函数
     * @param data 要反序列化的数据
     */
    explicit Deserializer(const std::vector<uint8_t>& data);

    /**
     * @brief 构造函数
     * @param data 数据指针
     * @param size 数据大小
     */
    Deserializer(const uint8_t* data, size_t size);

    /**
     * @brief 析构函数
     */
    ~Deserializer() = default;

    /**
     * @brief 反序列化一个字节
     * @param value 输出参数
     * @return 是否成功
     */
    bool Deserialize(uint8_t& value);

    /**
     * @brief 反序列化一个16位整数
     * @param value 输出参数
     * @return 是否成功
     */
    bool Deserialize(uint16_t& value);

    /**
     * @brief 反序列化一个32位整数
     * @param value 输出参数
     * @return 是否成功
     */
    bool Deserialize(uint32_t& value);

    /**
     * @brief 反序列化一个64位整数
     * @param value 输出参数
     * @return 是否成功
     */
    bool Deserialize(uint64_t& value);

    /**
     * @brief 反序列化一个32位浮点数
     * @param value 输出参数
     * @return 是否成功
     */
    bool Deserialize(float& value);

    /**
     * @brief 反序列化一个64位浮点数
     * @param value 输出参数
     * @return 是否成功
     */
    bool Deserialize(double& value);

    /**
     * @brief 反序列化一个布尔值
     * @param value 输出参数
     * @return 是否成功
     */
    bool Deserialize(bool& value);

    /**
     * @brief 反序列化一个字符串
     * @param str 输出参数
     * @return 是否成功
     */
    bool Deserialize(std::string& str);

    /**
     * @brief 反序列化原始数据块
     * @param output 输出缓冲区
     * @param size 数据大小
     * @return 是否成功
     */
    bool DeserializeRaw(void* output, size_t size);

    /**
     * @brief 反序列化一个容器
     * @param container 输出参数
     * @return 是否成功
     */
    template<typename T>
    bool DeserializeContainer(std::vector<T>& container);

    /**
     * @brief 获取剩余可读字节数
     * @return 字节数
     */
    size_t GetBytesRemaining() const;

    /**
     * @brief 检查是否已到达末尾
     * @return 如果已到达末尾则返回true
     */
    bool IsEOF() const;

    /**
     * @brief 重置反序列化器到起始位置
     */
    void Reset();

    /**
     * @brief 跳转到指定位置
     * @param position 目标位置（字节偏移）
     * @return 是否成功
     */
    bool Seek(size_t position);

private:
    const uint8_t* data_;
    size_t dataSize_;
    size_t position_;

    /**
     * @brief 读取一个字节
     * @param value 输出参数
     * @return 是否成功
     */
    bool ReadByte(uint8_t& value);

    /**
     * @brief 将网络字节序转换为主机字节序（16位）
     */
    static uint16_t NetworkToHost(uint16_t value);
    
    /**
     * @brief 将网络字节序转换为主机字节序（32位）
     */
    static uint32_t NetworkToHost(uint32_t value);
    
    /**
     * @brief 将网络字节序转换为主机字节序（64位）
     */
    static uint64_t NetworkToHost(uint64_t value);
};

// ================================================================
// 辅助宏
// ================================================================

/**
 * @brief 序列化助手宏
 * @param serializer 序列化器引用
 * @param value 要序列化的值
 * @return 是否成功
 */
#define SERIALIZE(serializer, value) ((serializer).Serialize(value))

/**
 * @brief 反序列化助手宏
 * @param deserializer 反序列化器引用
 * @param value 输出参数
 * @return 是否成功
 */
#define DESERIALIZE(deserializer, value) ((deserializer).Deserialize(value))

// ================================================================
// 模板实现
// ================================================================

template<typename T>
bool Serializer::SerializeContainer(const std::vector<T>& container) {
    // 序列化容器大小
    uint32_t size = static_cast<uint32_t>(container.size());
    if (!Serialize(size)) {
        return false;
    }
    
    // 序列化每个元素
    for (const auto& item : container) {
        // 对基本类型直接使用内存拷贝
        if (!SerializeRaw(&item, sizeof(T))) {
            return false;
        }
    }
    
    return true;
}

template<typename T>
bool Deserializer::DeserializeContainer(std::vector<T>& container) {
    // 反序列化容器大小
    uint32_t size;
    if (!Deserialize(size)) {
        return false;
    }
    
    // 调整容器大小
    container.resize(size);
    
    // 反序列化每个元素
    for (auto& item : container) {
        if (!DeserializeRaw(&item, sizeof(T))) {
            container.clear();
            return false;
        }
    }
    
    return true;
}

} // namespace PhysicsSync
