# PhysicsSync - 带渲染表现的物理同步方案

## 项目概述

PhysicsSync 是一套基于 **Jolt Physics**、**Google Filament** 和 **KCP** 协议的物理同步技术方案，专为多人在线游戏设计。

### 核心特性

- **服务器权威架构** - 服务器作为物理世界的唯一权威，确保游戏公平性
- **客户端预测与校正** - 低延迟体验 + 准确的状态同步
- **确定性物理模拟** - 固定时间步长确保跨平台一致性
- **高效网络传输** - KCP 协议提供低延迟可靠传输
- **平滑渲染** - 插值算法消除抖动和瞬移

## 技术栈

| 组件 | 技术 | 用途 |
|------|------|------|
| 物理引擎 | [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | 确定性物理模拟 |
| 渲染库 | [Google Filament](https://github.com/google/filament) | 高性能实时渲染 |
| 网络协议 | [KCP](https://github.com/skywind3000/kcp) | 快速可靠传输 |
| 测试框架 | [Google Test](https://github.com/google/googletest) | 单元测试 |
| 构建系统 | CMake | 跨平台构建 |

## 三方库清单

| 库名 | 版本 | 许可证 | 路径 |
|------|------|--------|------|
| Jolt Physics | PhysicsJolt 分支 | Zlib | `external/JoltPhysics/` |
| Google Filament | main 分支 | Apache-2.0 | `external/filament/` |
| KCP | master 分支 | BSD-3 | `external/kcp/` |
| Google Test | v1.14.0 | BSD-3 | FetchContent 自动下载 |

### Jolt Physics

> 高性能 C++ 物理引擎，支持多线程和确定性模拟。
> 
> 特性：
> - 支持刚体、碰撞体、约束系统
> - 多线程物理模拟
> - 跨平台确定性（可通过日志验证）
> - 轻量级、高性能

```bash
# 更新 Jolt Physics
cd external/JoltPhysics && git pull
```

### Google Filament

> Google 开源的实时物理渲染引擎，支持 PBR 材质和高级光照。
>
> 特性：
> - PBR 渲染管线
> - 基于物理的材质系统
> - 全局光照（IBL）
> - 跨平台（Windows/Linux/macOS/Android/iOS）

```bash
# 更新 Filament
cd external/filament && git pull
```

### KCP

> 快速可靠传输协议，基于 UDP 实现，比 TCP 延迟更低。
>
> 特性：
> - 低延迟（RTT 降低 30%-40%）
> - 可靠传输
> - 流量控制
> - 单文件集成

```bash
# 更新 KCP
cd external/kcp && git pull
```

## 项目结构

```
PhysicsSync/
├── CMakeLists.txt              # 顶层 CMake 构建配置
├── external/                   # 三方库目录
│   ├── JoltPhysics/            # Jolt Physics 物理引擎
│   ├── filament/               # Google Filament 渲染引擎
│   └── kcp/                    # KCP 快速可靠传输协议
├── src/
│   ├── common/                 # 公共库
│   │   ├── physics_state.h/.cpp     # 物理状态定义
│   │   ├── serializer.h/.cpp        # 二进制序列化/反序列化
│   │   ├── timestep_manager.h/.cpp  # 确定性时间步长管理器
│   │   ├── deterministic_random.h/.cpp # 确定性随机数（PCG32）
│   │   ├── kcp_wrapper.h            # KCP 协议 C++ 封装
│   │   └── network_protocol.h       # 网络协议定义
│   ├── server/               # 服务器模块
│   │   ├── physics_server.h/.cpp    # 物理服务器核心
│   │   ├── server_main.cpp          # 服务器主程序
│   │   └── server_tests.cpp         # 服务器单元测试
│   └── client/               # 客户端模块
│       ├── physics_client.h/.cpp    # 客户端预测与校正
│       └── client_main.cpp          # 客户端主程序
├── build.bat                   # Windows 构建脚本
├── start_server.bat            # Windows 服务器启动脚本
├── start_client.bat            # Windows 客户端启动脚本
├── tips.txt                    # 任务描述
└── README.md                   # 项目文档
```

## 快速开始

### Windows

1. 确保已安装以下工具：
   - Visual Studio 2019 或更高版本（含 CMake 工具）
   - Git

2. 构建项目：
```bash
build.bat --build Release --test
```

3. 启动服务器：
```bash
start_server.bat
```

4. 启动客户端：
```bash
start_client.bat
```

### Linux

1. 确保已安装以下工具：
   - CMake 3.16+
   - GCC 9+ 或 Clang 10+
   - Git

2. 构建项目：
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

3. 运行测试：
```bash
ctest --output-on-failure
```

4. 运行服务器和客户端：
```bash
./bin/PhysicsSyncServerMain
./bin/PhysicsSyncClientMain
```

## 架构设计

### 核心组件

1. **时间步长管理器** - 确保物理模拟以固定频率运行
2. **物理状态** - 包含刚体的完整状态信息
3. **序列化器** - 高效二进制序列化
4. **确定性随机数** - PCG32 算法实现跨平台一致
5. **KCP 封装** - 基于 KCP 协议的可靠网络传输

### 同步流程

```
客户端 ----玩家输入----> 服务器
客户端 <---世界快照---- 服务器
  │                        │
  ▼                        ▼
状态校正              广播状态
```

### 物理同步时序

```
时间轴：

服务器: [物理步] [物理步] [物理步] [物理步]
              |          |          |
              v          v          v
          快照N+1    快照N+2    快照N+3
              |          |          |
客户端:     |<--------收到快照-------->|
              |
         [预测 N+1] [校正+预测 N+2]
```

## 许可证

本项目使用 MIT 许可证。各三方库遵循各自的许可证：
- Jolt Physics: Zlib
- Google Filament: Apache-2.0
- KCP: BSD-3
- Google Test: BSD-3
