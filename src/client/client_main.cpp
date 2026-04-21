/**
 * @file client_main.cpp
 * @brief 客户端主程序入口 - 集成物理模拟、网络通信和 Filament 渲染
 *
 * 启动物理客户端，连接到服务器（默认 127.0.0.1:9300），
 * 运行物理模拟、网络循环和渲染循环。
 *
 * 功能：
 * 1. 连接到 PhysicsSync 服务器
 * 2. 接收物理状态快照并映射到渲染对象
 * 3. WASD 键盘控制玩家物体
 * 4. 可选的 Filament 渲染输出
 */

#include "physics_client.h"
#include "../common/physics_state.h"
#include "../common/timestep_manager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <atomic>

namespace PhysicsSync {

// ================================================================
// 全局变量
// ================================================================

PhysicsClient* g_client = nullptr;
std::atomic<bool> g_running{true};

/**
 * @brief 信号处理器 - 优雅退出
 */
void SignalHandler(int signal) {
    (void)signal;
    g_running = false;
    if (g_client && g_client->IsConnected()) {
        std::cout << "\n[Main] Received signal " << signal
                  << ", shutting down..." << std::endl;
        g_client->Stop();
    }
}

} // namespace PhysicsSync

#ifdef RENDERING_ENABLED
// === ================================================================
// 渲染路径（Filament 可用时）
// === ================================================================

#include "FilamentRenderer.h"
#include "RenderingObjectManager.h"
#include "InputHandler.h"

#include <windows.h>

// 前向声明 Windows 消息处理函数
LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 全局渲染对象
static PhysicsSync::FilamentRenderer g_renderer;
static PhysicsSync::RenderingObjectManager g_renderMgr{nullptr, nullptr};
static PhysicsSync::InputHandler g_inputHandler;

/**
 * @brief 获取原生窗口句柄的回调
 */
static void* GetWindowHandle() {
    return GetForegroundWindow();
}

/**
 * @brief 渲染回调 - 从 PhysicsClient 每帧调用
 *
 * 此函数在模拟线程中被调用，负责：
 * 1. 同步服务器快照到渲染对象
 * 2. 处理玩家输入
 * 3. 执行 Filament 渲染
 */
static void RenderCallback(uint32_t timestamp, float interpolationAlpha) {
    if (!g_client || !g_client->IsRunning()) return;

    // 1. 获取当前物理世界状态
    PhysicsSync::PhysicsWorldSnapshot worldState;
    if (g_client->GetWorldState(worldState)) {
        // 2. 同步物理状态到渲染对象
        g_renderMgr.syncExistingFromSnapshot(worldState);
        g_renderMgr.applyUpdates();
    }

    // 3. 更新输入处理器
    g_inputHandler.update();

    // 4. 获取玩家输入并发送到服务器
    PhysicsSync::PlayerInput input = g_inputHandler.getCurrentInput();
    if (input.moveX != 0.0f || input.moveY != 0.0f || input.buttons != 0) {
        g_client->SendInput(input);
    }

    // 5. 渲染帧
    g_renderer.renderFrame(
        static_cast<int32_t>(g_renderer.getConfig().width),
        static_cast<int32_t>(g_renderer.getConfig().height));
}

/**
 * @brief 完整的 Windows 程序入口 - 带 Filament 渲染
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    using namespace PhysicsSync;

    signal(SIGINT, SignalHandler);

    // 解析命令行参数
    std::string serverHost = "127.0.0.1";
    uint16_t port = 9300;

    for (int i = 1; i < _argc; i++) {
        if (strcmp(_argv[i], "--server") == 0 && i + 1 < _argc) {
            serverHost = _argv[++i];
        } else if (strcmp(_argv[i], "--port") == 0 && i + 1 < _argc) {
            port = static_cast<uint16_t>(std::atoi(_argv[++i]));
        } else if (strcmp(_argv[i], "--help") == 0) {
            PrintUsage(_argv[0]);
            return 0;
        }
    }

    std::cout << "===== PhysicsSync Client (with Rendering) =====" << std::endl;
    std::cout << "  Server: " << serverHost << std::endl;
    std::cout << "  Port:   " << port << std::endl;
    std::cout << "==============================================" << std::endl;

    // -- 1. 初始化 Filament 渲染引擎 --
    RendererConfig renderConfig(1280, 720, true);
    renderConfig.windowTitle = "PhysicsSync Client";
    renderConfig.vsync = false;

    if (!g_renderer.init(renderConfig, GetWindowHandle)) {
        std::cerr << "[Main] Failed to initialize Filament renderer." << std::endl;
        return 1;
    }

    // 设置相机位置
    g_renderer.setCameraPosition(0.0f, 5.0f, 15.0f);

    // -- 2. 初始化渲染对象管理器 --
    g_renderMgr = RenderingObjectManager(g_renderer.getEngine(), g_renderer.getScene());
    g_renderMgr.initialize();

    // -- 3. 初始化输入处理器 --
    InputConfig inputConfig;
    inputConfig.moveSpeed = 5.0f;
    inputConfig.lookSensitivity = 0.002f;
    g_inputHandler.initialize(inputConfig);

    // -- 4. 创建并启动物理客户端 --
    ClientConfig config(serverHost, port);
    PhysicsClient client(config, RenderCallback);
    g_client = &client;

    if (!client.Initialize()) {
        std::cerr << "[Main] Failed to initialize client!" << std::endl;
        return 1;
    }

    std::cout << "[Main] Client is running. Press Ctrl+C to stop." << std::endl;
    std::cout << "[Main] Controls: WASD to move, Shift to sprint" << std::endl;

    // -- 5. 启动客户端 --
    client.Start();

    // -- 6. 主循环：处理 Windows 消息并渲染 --
    MSG msg;
    int frameCount = 0;
    auto lastStatsTime = std::chrono::steady_clock::now();

    while (g_running.load()) {
        // 处理 Windows 消息（包含键盘输入）
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // 渲染帧
        g_renderer.renderFrame(
            static_cast<int32_t>(g_renderer.getConfig().width),
            static_cast<int32_t>(g_renderer.getConfig().height));

        frameCount++;

        // 每秒打印一次统计信息
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStatsTime).count() >= 1) {
            lastStatsTime = now;
            std::cout << "[Main] Frame " << frameCount
                      << " | " << client.GetStatistics() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // -- 7. 清理 --
    client.Stop();

    std::cout << "\n[Main] Final statistics:" << std::endl;
    std::cout << client.GetStatistics() << std::endl;

    g_renderer.destroy();
    g_renderMgr.destroy();
    g_inputHandler.destroy();
    g_client = nullptr;

    std::cout << "[Main] Client stopped." << std::endl;
    return 0;
}

void PrintUsage(const char* programName) {
    std::cout << "Usage: " << programName
              << " [options]" << std::endl
              << "Options:" << std::endl
              << "  --server <host>   Server address (default: 127.0.0.1)" << std::endl
              << "  --port <n>        Server port (default: 9300)" << std::endl
              << "  --help            Show this help message" << std::endl;
}

#else
// === ================================================================
// 无渲染路径（Filament 不可用时）
// === ================================================================

/**
 * @brief 打印使用说明
 */
void PrintUsage(const char* programName) {
    std::cout << "Usage: " << programName
              << " [options]" << std::endl
              << "Options:" << std::endl
              << "  --server <host>   Server address (default: 127.0.0.1)" << std::endl
              << "  --port <n>        Server port (default: 9300)" << std::endl
              << "  --help            Show this help message" << std::endl;
}

/**
 * @brief 模拟渲染回调 - 无渲染时使用
 */
static void MockRenderCallback(uint32_t timestamp, float interpolationAlpha) {
    static int frameCount = 0;
    frameCount++;

    if (frameCount % 100 == 0) {
        std::cout << "[Render] Frame " << frameCount
                  << " (tick: " << timestamp
                  << ", alpha: " << interpolationAlpha << ")" << std::endl;
    }
}

/**
 * @brief 标准 main 入口 - 无 Filament 渲染
 */
int main(int argc, char* argv[]) {
    using namespace PhysicsSync;

    signal(SIGINT, SignalHandler);

    // 解析命令行参数
    std::string serverHost = "127.0.0.1";
    uint16_t port = 9300;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0 && i + 1 < argc) {
            serverHost = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    std::cout << "===== PhysicsSync Client (No Rendering) =====" << std::endl;
    std::cout << "  Server: " << serverHost << std::endl;
    std::cout << "  Port:   " << port << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << "  Note: Filament rendering not available." << std::endl;

    // 创建并初始化客户端
    ClientConfig config(serverHost, port);
    PhysicsClient client(config, MockRenderCallback);
    g_client = &client;

    if (!client.Initialize()) {
        std::cerr << "Failed to initialize client!" << std::endl;
        return 1;
    }

    std::cout << "\n[Main] Client is running. Press Ctrl+C to stop." << std::endl;

    // 启动客户端
    client.Start();

    // 模拟输入线程 - 产生正弦波移动
    std::thread inputThread([&client]() {
        PlayerInput input(1, 0);
        int tick = 0;

        while (g_running.load()) {
            if (!client.IsConnected()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            input.moveX = std::sin(tick * 0.1f) * 0.5f;
            input.moveY = 0.0f;
            input.lookX = 0.0f;
            input.lookY = 0.0f;
            input.inputTick = static_cast<uint32_t>(tick);
            input.ComputeHash();

            client.SendInput(input);
            tick++;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    });

    // 主循环
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 停止客户端
    client.Stop();

    if (inputThread.joinable()) {
        inputThread.join();
    }

    g_client = nullptr;

    std::cout << "\n[Main] Client statistics:" << std::endl;
    std::cout << client.GetStatistics() << std::endl;
    std::cout << "\n[Main] Client stopped." << std::endl;
    return 0;
}

#endif
