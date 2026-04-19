/**
 * @file server_main.cpp
 * @brief 服务器主程序入口
 *
 * 启动物理服务器，监听网络端口（默认 9300），接收客户端连接。
 */

#include "physics_server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstring>
#include <cstdlib>

namespace {
    PhysicsSync::PhysicsServer* g_server = nullptr;

    void SignalHandler(int signal) {
        (void)signal;
        if (g_server && g_server->IsRunning()) {
            std::cout << "\n[Main] Received signal " << signal
                      << ", shutting down..." << std::endl;
            g_server->Stop();
        }
    }

    void PrintUsage(const char* programName) {
        std::cout << "Usage: " << programName
                  << " [options]" << std::endl
                  << "Options:" << std::endl
                  << "  --port <n>        Listening port (default: 9300)" << std::endl
                  << "  --physics-hz <n>  Physics update frequency (default: 60)" << std::endl
                  << "  --snapshot-hz <n> Snapshot broadcast frequency (default: 30)" << std::endl
                  << "  --help            Show this help message" << std::endl;
    }
} // anonymous namespace

int main(int argc, char* argv[]) {
    using namespace PhysicsSync;

    // 设置信号处理
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    // 解析命令行参数
    uint16_t port = 9300;
    uint32_t physicsHz = 60;
    uint32_t snapshotHz = 30;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (strcmp(argv[i], "--physics-hz") == 0 && i + 1 < argc) {
            physicsHz = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--snapshot-hz") == 0 && i + 1 < argc) {
            snapshotHz = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << "   PhysicsSync Server" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Port:         " << port << std::endl;
    std::cout << "  Physics Hz:   " << physicsHz << std::endl;
    std::cout << "  Snapshot Hz:  " << snapshotHz << std::endl;
    std::cout << "========================================" << std::endl;

    // 创建并初始化服务器
    ServerConfig config(port);
    config.physicsHz = physicsHz;
    config.snapshotHz = snapshotHz;

    PhysicsServer server(config);

    if (!server.Initialize()) {
        std::cerr << "Failed to initialize server!" << std::endl;
        return 1;
    }

    g_server = &server;

    // 启动服务器
    server.Start();

    std::cout << "\n[Main] Server is running. Press Ctrl+C to stop." << std::endl;

    // 主循环 — 每 5 秒打印统计
    while (server.IsRunning()) {
        static auto lastStatsTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        if ((now - lastStatsTime).count() > 5.0) {
            std::cout << "\n" << server.GetStatistics() << std::endl;
            lastStatsTime = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    g_server = nullptr;

    std::cout << "\n[Main] Server stopped." << std::endl;
    return 0;
}
