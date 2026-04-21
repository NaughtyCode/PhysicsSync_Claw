# === ======================================== ========================
# src/client/CMakeLists.txt - 客户端构建配置
# === ======================================== ========================
# 包含：物理客户端、Filament渲染封装、输入处理器
# === ======================================== ========================

# 客户端库源文件 - 基础部分（始终包含）
set(CLIENT_BASE_SOURCES
    physics_client.cpp
    physics_client.h
    InputHandler.cpp
    InputHandler.h
)

# Filament 渲染源文件（条件编译）
if(EXISTS ${EXTERNAL_DIR}/filament)
    set(CLIENT_RENDERING_SOURCES
        FilamentRenderer.cpp
        FilamentRenderer.h
        RenderingObjectManager.cpp
        RenderingObjectManager.h
    )
    set(RENDERING_AVAILABLE ON)
    message(STATUS "Google Filament found - rendering enabled")
else()
    set(CLIENT_RENDERING_SOURCES)
    set(RENDERING_AVAILABLE OFF)
    message(WARNING "Filament not found - client will build without rendering")
endif()

# 合并所有源文件
set(CLIENT_SOURCES
    ${CLIENT_BASE_SOURCES}
    ${CLIENT_RENDERING_SOURCES}
)

# 客户端库
add_library(PhysicsSyncClient STATIC
    ${CLIENT_SOURCES}
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

# Google Filament 渲染引擎集成
if(RENDERING_AVAILABLE)
    # 添加 Filament 头文件路径
    target_include_directories(PhysicsSyncClient
        PRIVATE
            ${EXTERNAL_DIR}/filament/filament/include
            ${EXTERNAL_DIR}/filament/libs/utils/include
            ${EXTERNAL_DIR}/filament/libs/math/include
            ${EXTERNAL_DIR}/filament/third_party/glm
    )

    # 链接 Filament 核心库（如果已构建）
    if(TARGET filament)
        target_link_libraries(PhysicsSyncClient PRIVATE filament)
        message(STATUS "Filament library linked")
    endif()

    if(TARGET engine)
        target_link_libraries(PhysicsSyncClient PRIVATE engine)
        message(STATUS "Engine library linked")
    endif()

    # 启用渲染支持的宏定义
    target_compile_definitions(PhysicsSyncClient
        PRIVATE RENDERING_ENABLED=1
    )
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

message(STATUS "PhysicsSyncClient built with rendering support")
