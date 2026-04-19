# === ======================================== ========================
# src/common/CMakeLists.txt - 公共库构建配置
# === ======================================== ========================
# 公共库包含：物理状态定义、序列化、网络协议、时间步长管理、KCP封装
# === ======================================== ========================

# 公共库源文件
set(COMMON_SOURCES
    physics_state.cpp
    physics_state.h
    serializer.cpp
    serializer.h
    network_protocol.h
    network_protocol.cpp
    network_layer.h
    network_layer.cpp
    udp_socket.h
    udp_socket.cpp
    kcp_wrapper.h
    timestep_manager.cpp
    timestep_manager.h
    deterministic_random.cpp
    deterministic_random.h
)

# KCP源文件（来自third_party）
set(KCP_SOURCE
    ${EXTERNAL_DIR}/kcp/ikcp.c
)

add_library(PhysicsSyncCommon STATIC
    ${COMMON_SOURCES}
    ${KCP_SOURCE}
)

target_include_directories(PhysicsSyncCommon
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${EXTERNAL_DIR}/kcp
)

# 链接Jolt Physics（如果可用）
if(TARGET Jolt)
    target_link_libraries(PhysicsSyncCommon PUBLIC Jolt)
    message(STATUS "Jolt Physics linked to PhysicsSyncCommon")
endif()

# 编译定义
target_compile_definitions(PhysicsSyncCommon
    PRIVATE
        # Windows特定定义
        $<$<CXX_COMPILER_ID:MSVC>:
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
        >
)

# 设置目标属性
set_target_properties(PhysicsSyncCommon PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# 单元测试
if(BUILD_TESTING)
    add_executable(PhysicsSyncCommonTests
        common_tests.cpp
    )
    
    target_link_libraries(PhysicsSyncCommonTests
        PRIVATE
            PhysicsSyncCommon
            GTest::gtest
            GTest::gtest_main
    )
    
    include(GoogleTest)
    gtest_discover_tests(PhysicsSyncCommonTests)
    
    # 测试依赖
    add_dependencies(PhysicsSyncCommonTests PhysicsSyncCommon)
endif()

message(STATUS "PhysicsSyncCommon built with KCP integration")
