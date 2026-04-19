/**
 * @file kcp_wrapper.h
 * @brief KCP协议封装 - 基于 skywind3000/kcp 实现
 * 
 * 本文件提供KCP协议的C++封装，用于快速可靠传输。
 * KCP是一种基于UDP的ARQ协议，具有低延迟和高可靠性的特点。
 * 
 * 底层实现：https://github.com/skywind3000/kcp
 * 
 * 特性：
 * - 不可靠UDP上的可靠传输
 * - 可配置的重传延迟
 * - 流量控制和拥塞控制
 * - 低延迟优化（nodelay模式）
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

// 包含KCP底层头文件
#include "ikcp.h"

namespace PhysicsSync {

// ===================================================================
// KCP配置
// ===================================================================

/**
 * @brief KCP配置结构体
 */
struct KCPConfig {
    uint32_t conv;                ///< 会话ID（用于多路复用）
    uint32_t mtu;                 ///< 最大传输单元（默认1400字节）
    int fastresend;               ///< 快速重传阈值（0表示禁用）
    int nodelay;                  ///< 无模式标志
    int interval;                 ///< 内部更新间隔（毫秒）
    int rcvwnd;                   ///< 接收窗口大小
    int sndwnd;                   ///< 发送窗口大小
    uint32_t rto_min;             ///< 最小RTO（毫秒）
    
    /**
     * @brief 默认配置
     */
    KCPConfig()
        : conv(0)
        , mtu(1400)
        , fastresend(0)
        , nodelay(0)
        , interval(10)
        , rcvwnd(128)
        , sndwnd(128)
        , rto_min(100)
    {}
};

/**
 * @brief 无模式配置（优化延迟）
 * 
 * 启用后KCP会禁用拥塞控制，以获得最低延迟。
 * 适合对延迟敏感的应用，如实时物理同步。
 * 
 * 参数：nodelay=1, interval=10, resend=2, nc=1
 */
inline KCPConfig GetNodelayConfig(uint32_t conv = 0) {
    KCPConfig config;
    config.conv = conv;
    config.nodelay = 1;       // 启用无模式
    config.interval = 10;     // 10ms内部更新
    config.fastresend = 2;    // 快速重传
    config.rto_min = 50;      // 50ms最小RTO
    return config;
}

/**
 * @brief 标准配置（平衡延迟和可靠性）
 */
inline KCPConfig GetStandardConfig(uint32_t conv = 0) {
    KCPConfig config;
    config.conv = conv;
    config.nodelay = 0;
    config.interval = 10;
    config.rto_min = 100;
    return config;
}

/**
 * @brief 高可靠配置（启用拥塞控制）
 */
inline KCPConfig GetReliableConfig(uint32_t conv = 0) {
    KCPConfig config;
    config.conv = conv;
    config.nodelay = 0;
    config.interval = 50;     // 50ms内部更新
    config.rcvwnd = 256;
    config.sndwnd = 256;
    config.fastresend = 0;
    config.rto_min = 200;
    return config;
}

// ===================================================================
// KCP回调类型
// ===================================================================

/**
 * @brief 发送回调函数类型
 */
using SendCallback = void(*)(const void* buffer, int size, void* user);

/**
 * @brief 日志回调函数类型
 */
using LogCallback = void(*)(int level, const char* message);

// ===================================================================
// KCP封装类
// ===================================================================

/**
 * @brief KCP协议封装类
 * 
 * 封装了IKCPCB，提供简洁的C++接口。
 * 
 * 使用示例：
 * @code
 *     // 创建回调类
 *     class MyKCPWriter : public KCPWriter {
 *         virtual int OnOutput(const char* buffer, int size) override {
 *             // 发送UDP包
 *             udpSocket.send(buffer, size);
 *             return 0;
 *         }
 *     };
 *     
 *     // 创建KCP实例
 *     KCPWrapper kcp(12345, &writer);
 *     kcp.SetConfig(GetNodelayConfig());
 *     
 *     // 定期调用Update
 *     while (running) {
 *         kcp.Update(GetTickCount64());
 *     }
 * @endcode
 */
class KCPWrapper {
public:
    /**
     * @brief 输出接口
     * 
     * 实现此接口以处理KCP的输出数据（即发送UDP包）。
     */
    class KCPWriter {
    public:
        virtual ~KCPWriter() = default;
        virtual int OnOutput(const char* buffer, int size) = 0;
    };

    /**
     * @brief 构造函数
     * @param conv 会话ID（通信双方必须一致）
     * @param writer 输出接口，用于发送数据
     */
    explicit KCPWrapper(uint32_t conv, KCPWriter* writer);
    
    /**
     * @brief 析构函数
     */
    ~KCPWrapper();
    
    // 禁止拷贝
    KCPWrapper(const KCPWrapper&) = delete;
    KCPWrapper& operator=(const KCPWrapper&) = delete;

    /**
     * @brief 应用配置
     * @param config 配置结构体
     */
    void SetConfig(const KCPConfig& config);

    /**
     * @brief 更新KCP状态
     * @param currentTick 当前时间（毫秒）
     * 
     * 必须定期调用（建议每10-100ms），以处理重传和拥塞控制。
     * 也可以使用UpdateTimer()获取下次调用时间。
     */
    void Update(uint32_t currentTick);

    /**
     * @brief 获取下次Update调用建议时间
     * @param currentTick 当前时间（毫秒）
     * @return 距下次调用的毫秒数，0表示应立即调用
     */
    uint32_t UpdateTimer(uint32_t currentTick);

    /**
     * @brief 发送数据
     * @param data 数据缓冲区
     * @param size 数据大小
     * @return 0表示成功，-1表示错误
     */
    int Send(const void* data, int size);

    /**
     * @brief 接收数据
     * @param buffer 输出缓冲区
     * @param size 缓冲区大小
     * @return 接收的字节数，-1表示错误，0表示没有数据
     */
    int Recv(void* buffer, int size);

    /**
     * @brief 处理传入的KCP包
     * @param data 包数据（UDP包）
     * @param size 包大小
     * @return 0表示成功
     */
    int Input(const void* data, int size);

    /**
     * @brief 检查待发送数据量
     * @return 等待发送的字节数
     */
    int WaitSnd() const;

    /**
     * @brief 检查下一条消息的大小
     * @return 消息大小，-1表示没有数据
     */
    int PeekSize() const;

    /**
     * @brief 设置MTU
     * @param mtu 最大传输单元
     * @return 0表示成功
     */
    int SetMTU(uint32_t mtu);

    /**
     * @brief 设置窗口大小
     * @param sndwnd 发送窗口
     * @param rcvwnd 接收窗口
     * @return 0表示成功
     */
    int SetWindowSize(int sndwnd, int rcvwnd);

    /**
     * @brief 重置KCP状态（重新建立连接时调用）
     */
    void Reset();

    /**
     * @brief 获取用户数据指针
     * @return 用户数据指针
     */
    void* GetUser() const { return user_; }
    
    /**
     * @brief 设置用户数据指针
     */
    void SetUser(void* user) { user_ = user; }

    /**
     * @brief 静态输出回调（供KCP底层调用）
     * @param buf 数据缓冲区
     * @param len 数据长度
     * @param kcp KCP控制块
     * @param user 用户数据（KCPWrapper实例）
     * @return 0表示成功
     */
    static int OnKCPOutput(const char* buf, int len, ikcpcb* kcp, void* user);

private:
    ikcpcb* kcp_;
    KCPWriter* writer_;
    void* user_;
};

// ===================================================================
// 内联实现
// ===================================================================

inline KCPWrapper::KCPWrapper(uint32_t conv, KCPWriter* writer)
    : kcp_(nullptr)
    , writer_(writer)
    , user_(nullptr)
{
    if (writer) {
        writer_ = writer;
        kcp_ = ikcp_create(conv, this);
        if (kcp_) {
            ikcp_setoutput(kcp_, &KCPWrapper::OnKCPOutput);
        }
    }
}

inline KCPWrapper::~KCPWrapper() {
    if (kcp_) {
        ikcp_release(kcp_);
        kcp_ = nullptr;
    }
}

inline void KCPWrapper::SetConfig(const KCPConfig& config) {
    if (!kcp_) return;
    
    ikcp_nodelay(kcp_, config.nodelay, config.interval, config.fastresend, 
                 config.rto_min > 0 ? 1 : 0);  // nc=1 if rto_min set
    ikcp_setmtu(kcp_, static_cast<int>(config.mtu));
    ikcp_wndsize(kcp_, config.sndwnd, config.rcvwnd);
}

inline void KCPWrapper::Update(uint32_t currentTick) {
    if (kcp_) {
        ikcp_update(kcp_, currentTick);
    }
}

inline uint32_t KCPWrapper::UpdateTimer(uint32_t currentTick) {
    if (!kcp_) return 0;
    return ikcp_check(kcp_, currentTick);
}

inline int KCPWrapper::Send(const void* data, int size) {
    if (!kcp_ || size <= 0) return -1;
    return ikcp_send(kcp_, static_cast<const char*>(data), size);
}

inline int KCPWrapper::Recv(void* buffer, int size) {
    if (!kcp_ || size <= 0) return -1;
    return ikcp_recv(kcp_, static_cast<char*>(buffer), size);
}

inline int KCPWrapper::Input(const void* data, int size) {
    if (!kcp_ || size <= 0) return -1;
    return ikcp_input(kcp_, static_cast<const char*>(data), size);
}

inline int KCPWrapper::WaitSnd() const {
    if (!kcp_) return -1;
    return ikcp_waitsnd(kcp_);
}

inline int KCPWrapper::PeekSize() const {
    if (!kcp_) return -1;
    return ikcp_peeksize(kcp_);
}

inline int KCPWrapper::SetMTU(uint32_t mtu) {
    if (!kcp_) return -1;
    return ikcp_setmtu(kcp_, static_cast<int>(mtu));
}

inline int KCPWrapper::SetWindowSize(int sndwnd, int rcvwnd) {
    if (!kcp_) return -1;
    return ikcp_wndsize(kcp_, sndwnd, rcvwnd);
}

inline void KCPWrapper::Reset() {
    // 释放并重新创建
    if (kcp_) {
        ikcp_release(kcp_);
        kcp_ = ikcp_create(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this) & 0xFFFF), this);
        if (kcp_) {
            ikcp_setoutput(kcp_, &KCPWrapper::OnKCPOutput);
        }
    }
}

inline int KCPWrapper::OnKCPOutput(const char* buf, int len, ikcpcb* kcp, void* user) {
    KCPWrapper* wrapper = static_cast<KCPWrapper*>(user);
    if (wrapper && wrapper->writer_) {
        return wrapper->writer_->OnOutput(buf, len);
    }
    return -1;
}

} // namespace PhysicsSync
