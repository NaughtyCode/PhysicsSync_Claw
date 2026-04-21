/**
 * @file InputHandler.h
 * @brief 输入处理器 - 键盘 (WASD) 与鼠标输入管理
 *
 * 本文件定义 InputHandler 类，负责：
 * 1. 捕获 Windows 键盘输入 (WASD 键)
 * 2. 将按键状态转换为游戏输入 (PlayerInput)
 * 3. 支持持续按键状态跟踪（用于平滑移动）
 * 4. 可选鼠标视角控制
 *
 * 使用示例：
 * @code
 *   InputHandler handler;
 *   handler.init(hwnd); // 初始化 Windows 句柄
 *   // 在游戏循环中：
 *   handler.update();
 *   PlayerInput input = handler.getInput();
 * @endcode
 */

#pragma once

#include "../common/physics_state.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

// Windows 前置声明
#ifdef _WIN32
    struct HINSTANCE__*;
    using HINSTANCE = HINSTANCE__*;
    using HWND = void*;
#endif

namespace PhysicsSync {

// ================================================================
// 辅助类型
// ================================================================

/**
 * @brief 二维向量（用于移动方向计算）
 */
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    float length() const { return std::sqrt(x * x + y * y); }
    Vec2 normalized() const {
        float len = length();
        return len > 1e-8f ? Vec2{x / len, y / len} : Vec2{0.0f, 0.0f};
    }
};

// ================================================================
// 输入配置
// ================================================================

/**
 * @brief 输入处理器配置
 */
struct InputConfig {
    float moveSpeed = 5.0f;           ///< 移动速度因子
    float lookSensitivity = 0.002f;   ///< 视角旋转灵敏度
    float maxMoveSpeed = 10.0f;       ///< 最大移动速度
    InputConfig() = default;
};

/**
 * @brief 按键状态枚举
 */
enum class KeyState {
    UP,    ///< 抬起
    DOWN,  ///< 刚按下
    HELD,  ///< 持续按住
};

/**
 * @brief Windows 窗口过程回调类型
 */
using WndProcCallback = std::function<bool(void* hwnd, uint32_t msg,
                                            uint64_t wParam, int64_t lParam)>;

// ================================================================
// 输入处理器
// ================================================================

/**
 * @brief 跨平台输入处理器
 *
 * 负责跟踪 WASD 按键状态，并将按键状态转换为 PlayerInput。
 * 在 Windows 上通过 Win32 API 处理键盘消息。
 */
class InputHandler {
public:
    InputHandler();
    ~InputHandler();
    InputHandler(const InputHandler&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;

    bool initialize(const InputConfig& config = InputConfig());
    void destroy();

    void update();
    void handleWindowsMessage(uint32_t msg, uint64_t wParam, int64_t lParam);

    PlayerInput getCurrentInput() const;
    KeyState getKeyState(uint8_t key) const;
    bool isKeyDown(uint8_t key) const;
    bool isKeyJustPressed(uint8_t key) const;

    const InputConfig& getConfig() const { return config_; }
    void setConfig(const InputConfig& config) { config_ = config; }
    bool isInitialized() const { return initialized_; }
    void setWndProcCallback(WndProcCallback cb) { wndProcCallback_ = std::move(cb); }

private:
    void updateKeyStates();
    Vec2 computeMoveVector() const;

    struct KeyEntry {
        KeyState currentState = KeyState::UP;
        KeyState previousState = KeyState::UP;
    };
    std::unordered_map<uint8_t, KeyEntry> keyStates_;
    InputConfig config_;
    bool initialized_ = false;
    WndProcCallback wndProcCallback_;
};

} // namespace PhysicsSync
