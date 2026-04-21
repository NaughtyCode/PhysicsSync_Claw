/**
 * @file InputHandler.cpp
 * @brief 输入处理器实现
 *
 * 实现 WASD 按键跟踪和输入转换逻辑。
 * 支持 Windows Win32 API 和跨平台回调两种模式。
 */

#include "InputHandler.h"
#include <iostream>
#include <cmath>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace PhysicsSync {

// ================================================================
// InputHandler 实现
// ================================================================

InputHandler::InputHandler() = default;

InputHandler::~InputHandler() {
    destroy();
}

bool InputHandler::initialize(const InputConfig& config) {
    if (initialized_) {
        std::cerr << "[InputHandler] Already initialized." << std::endl;
        return false;
    }

    config_ = config;
    initialized_ = true;
    std::cout << "[InputHandler] Initialized with moveSpeed="
              << config_.moveSpeed << std::endl;
    return true;
}

void InputHandler::destroy() {
    if (!initialized_) return;

    keyStates_.clear();
    initialized_ = false;
    std::cout << "[InputHandler] Destroyed." << std::endl;
}

void InputHandler::update() {
    if (!initialized_) return;

    updateKeyStates();
}

void InputHandler::handleWindowsMessage(uint32_t msg, uint64_t wParam, int64_t lParam) {
    if (!initialized_) return;

    // 只处理按键消息
    uint8_t vk = static_cast<uint8_t>(wParam);

    switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            auto& entry = keyStates_[vk];
            // 只有当按键从 UP 变为 DOWN 时才更新
            if (entry.currentState == KeyState::UP) {
                entry.previousState = KeyState::UP;
                entry.currentState = KeyState::DOWN;
            }
            break;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP: {
            auto it = keyStates_.find(vk);
            if (it != keyStates_.end()) {
                it->second.previousState = it->second.currentState;
                it->second.currentState = KeyState::UP;
            }
            break;
        }

        default:
            // 传递给窗口过程回调（如果有）
            if (wndProcCallback_) {
                wndProcCallback_(nullptr, msg, wParam, lParam);
            }
            break;
    }
}

PlayerInput InputHandler::getCurrentInput() const {
    PlayerInput input;

    // 计算移动向量
    Vec2 move = computeMoveVector();

    // 设置移动输入（归一化到 [-1, 1]）
    input.moveX = move.x;
    input.moveY = move.y;
    input.lookX = 0.0f;
    input.lookY = 0.0f;
    input.buttons = 0;

    // 如果有 Shift 按住，设置为奔跑按钮
    if (isKeyDown(0x10)) { // VK_SHIFT
        input.buttons |= 0x01; // Button 0: sprint
    }

    return input;
}

KeyState InputHandler::getKeyState(uint8_t key) const {
    auto it = keyStates_.find(key);
    if (it == keyStates_.end()) {
        return KeyState::UP;
    }
    return it->second.currentState;
}

bool InputHandler::isKeyDown(uint8_t key) const {
    KeyState state = getKeyState(key);
    return state == KeyState::DOWN || state == KeyState::HELD;
}

bool InputHandler::isKeyJustPressed(uint8_t key) const {
    return getKeyState(key) == KeyState::DOWN;
}

void InputHandler::updateKeyStates() {
    // 将所有 DOWN 状态转为 HELD（除了当前帧刚按下的）
    for (auto& [key, entry] : keyStates_) {
        if (entry.currentState == KeyState::DOWN) {
            entry.currentState = KeyState::HELD;
        }
    }
}

Vec2 InputHandler::computeMoveVector() const {
    Vec2 move{0.0f, 0.0f};

    // W/S: 前后移动 (Y 轴)
    if (isKeyDown(KEY_W)) {
        move.y += 1.0f;
    }
    if (isKeyDown(KEY_S)) {
        move.y -= 1.0f;
    }

    // A/D: 左右移动 (X 轴)
    if (isKeyDown(KEY_A)) {
        move.x -= 1.0f;
    }
    if (isKeyDown(KEY_D)) {
        move.x += 1.0f;
    }

    // 归一化对角线移动
    float len = move.length();
    if (len > 1.0f) {
        float invLen = 1.0f / len;
        move.x *= invLen;
        move.y *= invLen;
    }

    return move;
}

} // namespace PhysicsSync
