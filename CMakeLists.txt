# ============================================================================
# PhysicsSync - 基于 Jolt Physics + Google Filament + KCP 的物理同步方案
# ============================================================================
# 项目描述：
#   一套带渲染表现的物理同步技术方案，适用于多人在线游戏。
#   核心架构：服务器权威 + 客户端预测与校正
#
# 技术栈：
#   - 物理引擎：Jolt Physics (确定性物理模拟)
#   - 渲染库：Google Filament (高性能实时渲染)
#   - 网络协议：KCP (快速可靠传输协议)
#   - 语言：C++17
#
# 项目结构：
#   src/common/  - 公共代码（物理状态、序列化、网络协议、时间步长管理）
#   src/server/  - 服务器代码（物理服务器核心）
#   src/client/  - 客户端代码（渲染、预测校正、输入处理）
#   external/    - 三方库
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(PhysicsSync
    VERSION 1.0.0
    DESCRIPTION "Physics synchronization solution with rendering"
    LANGUAGES C CXX
)

# ============================================================================
# 全局编译设置
# ============================================================================

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 编译类型默认 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 编译器警告
if(MSVC)
    add_compile_options(/W4)
    add_definitions(/DNOMINMAX)
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# 第三方库路径配置
# ============================================================================

set(EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/external)

# ============================================================================
# Jolt Physics - 物理引擎
# ============================================================================
# 下载地址：https://github.com/jrouwe/JoltPhysics
#
# 集成说明：
#   Jolt Physics 通过 add_subdirectory 集成，使用其内置的 CMake 构建系统。
#   需要构建为静态库，并启用多线程支持（可选）。
#
# 构建选项：
#   JOLT_PHYSICS_STATIC     - 构建为静态库
#   JOLT_PHYSICS_SHARED_LIB - 构建为动态库（默认关闭）
#   JOLT_DEBUG_VISUALIZATION - 调试可视化（默认关闭）
#
# 使用说明：
#   include_directories(external/JoltPhysics/Include)
#   target_link_libraries(your_target Jolt)

if(EXISTS ${EXTERNAL_DIR}/JoltPhysics)
    message(STATUS "Jolt Physics found at: ${EXTERNAL_DIR}/JoltPhysics")
    # Jolt Physics has CMakeLists.txt in Build/ subdirectory
    add_subdirectory(${EXTERNAL_DIR}/JoltPhysics/Build BuildOut)
else()
    message(WARNING "Jolt Physics not found. Please clone into external/JoltPhysics")
    message(WARNING "  git clone https://github.com/jrouwe/JoltPhysics.git external/JoltPhysics")
    add_library(Jolt INTERFACE)
    add_library(Jolt::Jolt ALIAS Jolt)
endif()

# ============================================================================
# KCP - 快速可靠传输协议
# ============================================================================
# 下载地址：https://github.com/skywind3000/kcp
#
# 集成说明：
#   KCP 是一个轻量级的快速可靠ARQ协议，基于UDP实现。
#   本项目将其源码直接集成，通过 ikcp.h/ikcp.c 接口使用。
#
# 特性：
#   - 低延迟（比TCP更快）
#   - 可靠传输（支持ACK确认）
#   - 流量控制（滑动窗口）
#   - FEC前向纠错（可选）
#
# 使用说明：
#   include_directories(external/kcp)
#   target_sources(your_target PRIVATE external/kcp/ikcp.c)

if(EXISTS ${EXTERNAL_DIR}/kcp/ikcp.h)
    message(STATUS "KCP found at: ${EXTERNAL_DIR}/kcp")
    # KCP 是纯C库，直接在需要时包含其源文件
    set(KCP_HEADER ${EXTERNAL_DIR}/kcp/ikcp.h)
    set(KCP_SOURCE ${EXTERNAL_DIR}/kcp/ikcp.c)
else()
    message(WARNING "KCP not found. Please clone into external/kcp")
    message(WARNING "  git clone https://github.com/skywind3000/kcp.git external/kcp")
endif()

# ============================================================================
# Google Filament - 高性能实时渲染引擎
# ============================================================================
# 下载地址：https://github.com/google/filament
#
# 集成说明：
#   Filament 是 Google 开源的实时渲染引擎，支持 PBR 材质和高级光照。
#   它是一个大型库，需要 cmake 的 subproject 方式构建。
#
# 前置依赖（Filament 需要）：
#   - glslang (SPIR-V 着色器编译)
#   - Slang (着色器编程语言)
#   - libpng, libjpeg, zstd
#   - nanovg, nanoprintf
#   - tinygltf
#
# 构建选项：
#   FILAMENT_ENABLE_LUNAR   - 启用 Lunar 文档工具
#   FILAMENT_ENABLE_LOGN_16BIT_TONE_MAPPING - 启用 LogN 色调映射
#
# 使用说明：
#   include_directories(external/filament/include)
#   target_link_libraries(your_target filament engine math utils)

if(EXISTS ${EXTERNAL_DIR}/filament)
    message(STATUS "Filament found at: ${EXTERNAL_DIR}/filament")
    # Filament 需要特殊的构建配置，此处仅记录位置
    # 实际使用时需要完整配置所有依赖
    set(FILAMENT_DIR ${EXTERNAL_DIR}/filament)
else()
    message(WARNING "Filament not found. Please clone into external/filament")
    message(WARNING "  git clone https://github.com/google/filament.git external/filament")
endif()

# ============================================================================
# Google Test (用于单元测试)
# ============================================================================

option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # 对于 Windows，防止 GTest 覆盖我们的编译器标志
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# ============================================================================
# 添加子模块
# ============================================================================

# 公共库
add_subdirectory(src/common)

# 服务器
add_subdirectory(src/server)

# 客户端
add_subdirectory(src/client)

# ============================================================================
# 自动化测试
# ============================================================================

if(BUILD_TESTING)
    # 创建自动化测试目标
    add_custom_target(run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS PhysicsSyncCommonTests PhysicsSyncServerTests
        COMMENT "Running all unit tests..."
    )
endif()