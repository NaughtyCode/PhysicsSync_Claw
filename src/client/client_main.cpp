/**
 * @file client_main.cpp
 * @brief 客户端主程序入口
 *
 * 启动物理客户端，连接到服务器（默认 127.0.0.1:9300），
 * 运行物理模拟和渲染循环。
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

namespace {
    PhysicsSync::PhysicsClient* g_client = nullptr;

    void SignalHandler(int signal) {
        (void)signal;
        if (g_client && g_client->IsConnected()) {
            std::cout << "\n[Main] Received signal " << signal
                      << ", shutting down..." << std::endl;
            g_client->Disconnect();
        }
    }

    void MockRenderCallback(uint32_t timestamp, float interpolationAlpha) {
        static int frameCount = 0;
        frameCount++;

        if (frameCount % 100 == 0) {
            std::cout << "[Render] Frame " << frameCount
                      << " (tick: " << timestamp
                      << ", alpha: " << interpolationAlpha << ")" << std::endl;
        }
    }

    void PrintUsage(const char* programName) {
        std::cout << "Usage: " << programName
                  << " [options]" << std::endl
                  << "Options:" << std::endl
                  << "  --server <host>   Server address (default: 127.0.0.1)" << std::endl
                  << "  --port <n>        Server port (default: 9300)" << std::endl
                  << "  --help            Show this help message" << std::endl;
    }
} // anonymous namespace

int main(int argc, char* argv[]) {
    using namespace PhysicsSync;

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

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

    std::cout << "========================================" << std::endl;
    std::cout << "   PhysicsSync Client" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Server: " << serverHost << std::endl;
    std::cout << "  Port:   " << port << std::endl;
    std::cout << "========================================" << std::endl;

    // 创建并初始化客户端
    ClientConfig config(serverHost, port);
    PhysicsClient client(config, MockRenderCallback);

    if (!client.Initialize()) {
        std::cerr << "Failed to initialize client!" << std::endl;
        return 1;
    }

    g_client = &client;

    // 连接到服务器
    if (!client.Connect()) {
        std::cerr << "Failed to connect to server!" << std::endl;
        return 1;
    }

    std::cout << "\n[Main] Client is running. Press Ctrl+C to stop." << std::endl;
    std::cout << "[Main] Note: This is a demo client without Filament rendering." << std::endl;

    // 模拟输入线程 - 产生正弦波移动
    std::thread inputThread([&client, &serverHost, port]() {
        PlayerInput input(1, 0);
        int tick = 0;

        while (client.IsConnected()) {
            // 模拟移动输入（正弦波）
            input.moveX = std::sin(tick * 0.1f) * 0.5f;
            input.moveY = 0.0f;
            input.lookX = 0.0f;
            input.lookY = 0.0f;
            input.inputTick = static_cast<uint32_t>(tick);
            input.ComputeHash();

            if (!client.SendInput(input)) {
                std::cout << "[Input] Send failed, server not connected?" << std::endl;
            }

            tick++;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60Hz
        }
    });

    // 运行主循环
    client.Run();

    if (inputThread.joinable()) {
        inputThread.join();
    }

    g_client = nullptr;

    std::cout << "\n[Main] Client statistics:" << std::endl;
    std::cout << client.GetStatistics() << std::endl;

    std::cout << "\n[Main] Client stopped." << std::endl;
    return 0;
}
