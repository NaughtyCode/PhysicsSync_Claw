# === ======================================== ========================
# src/client/CMakeLists.txt - 客户端构建配置
# === ======================================== ========================

# 客户端库
add_library(PhysicsSyncClient STATIC
    physics_client.cpp
    physics_client.h
)

target_include_directories(PhysicsSyncClient
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# 链接公共库和Jolt Physics
target_link_libraries(PhysicsSyncClient
    PUBLIC
        PhysicsSyncCommon
)

if(TARGET Jolt)
    target_link_libraries(PhysicsSyncClient PUBLIC Jolt)
    message(STATUS "Jolt Physics linked to PhysicsSyncClient")
endif()

# 编译器定义
target_compile_definitions(PhysicsSyncClient
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
        >
)

# 客户端主程序
add_executable(PhysicsSyncClientMain
    client_main.cpp
)

target_link_libraries(PhysicsSyncClientMain
    PRIVATE
        PhysicsSyncClient
        PhysicsSyncCommon
)

if(TARGET Jolt)
    target_link_libraries(PhysicsSyncClientMain PRIVATE Jolt)
endif()

message(STATUS "PhysicsSyncClient built successfully")
