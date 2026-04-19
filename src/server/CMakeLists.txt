# === ======================================== ========================
# src/server/CMakeLists.txt - 服务器构建配置
# === ======================================== ========================

# 服务器库
add_library(PhysicsSyncServer STATIC
    physics_server.cpp
    physics_server.h
)

target_include_directories(PhysicsSyncServer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# 链接公共库和Jolt Physics
target_link_libraries(PhysicsSyncServer
    PUBLIC
        PhysicsSyncCommon
)

# 链接Jolt Physics（如果可用）
if(TARGET Jolt)
    target_link_libraries(PhysicsSyncServer PUBLIC Jolt)
    message(STATUS "Jolt Physics linked to PhysicsSyncServer")
endif()

# 编译器定义
target_compile_definitions(PhysicsSyncServer
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
        >
)

# 服务器主程序
add_executable(PhysicsSyncServerMain
    server_main.cpp
)

target_link_libraries(PhysicsSyncServerMain
    PRIVATE
        PhysicsSyncServer
        PhysicsSyncCommon
)

if(TARGET Jolt)
    target_link_libraries(PhysicsSyncServerMain PRIVATE Jolt)
endif()

# 服务器测试
if(BUILD_TESTING)
    add_executable(PhysicsSyncServerTests
        server_tests.cpp
    )
    
    target_link_libraries(PhysicsSyncServerTests
        PRIVATE
            PhysicsSyncServer
            PhysicsSyncCommon
            GTest::gtest
            GTest::gtest_main
    )
    
    if(TARGET Jolt)
        target_link_libraries(PhysicsSyncServerTests PRIVATE Jolt)
    endif()
    
    include(GoogleTest)
    gtest_discover_tests(PhysicsSyncServerTests)
    
    add_dependencies(PhysicsSyncServerTests PhysicsSyncCommon PhysicsSyncServer)
endif()

message(STATUS "PhysicsSyncServer built successfully")
