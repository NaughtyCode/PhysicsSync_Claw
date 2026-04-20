@echo off
REM === ==================================================
REM PhysicsSync 客户端启动脚本 (Windows)
REM === ==================================================
REM 
REM 使用方法:
REM   start_client.bat [选项]
REM 
REM 选项:
REM   --server <地址>     服务器地址 (默认: localhost)
REM   --port <端口号>     服务器端口 (默认: 7777)
REM   --help              显示帮助信息
REM 
REM === ==================================================

setlocal enabledelayedexpansion

REM 默认参数
set SERVER=localhost
set PORT=9300
set BUILD_TYPE=Release

REM 解析命令行参数
:parse_args
if "%~1"=="" goto :end_parse
if "%~1"=="--server" (
    set SERVER=%~2
    shift /2
    goto :parse_args
)
if "%~1"=="--port" (
    set PORT=%~2
    shift /2
    goto :parse_args
)
if "%~1"=="--build" (
    set BUILD_TYPE=%~2
    shift /2
    goto :parse_args
)
if "%~1"=="--help" (
    goto :show_help
)
shift
goto :parse_args

:end_parse

REM 显示启动信息
echo ========================================
echo    PhysicsSync Client (Windows)
echo ========================================
echo.
echo  配置参数:
echo    服务器地址:      %SERVER%
echo    服务器端口:      %PORT%
echo    构建类型:        %BUILD_TYPE%
echo.

REM 检查构建目录
if not exist "build\bin" (
    echo [提示] 构建目录不存在，正在构建...
    call :build_project
    if errorlevel 1 (
        echo [错误] 构建失败!
        pause
        exit /b 1
    )
)

REM 检查可执行文件
if not exist "build\bin\%BUILD_TYPE%\PhysicsSyncClientMain.exe" (
    echo [错误] 找不到客户端可执行文件!
    echo [提示] 请先运行: build.bat
    pause
    exit /b 1
)

echo [启动] 正在启动客户端...
echo.

REM 启动客户端
build\bin\%BUILD_TYPE%\PhysicsSyncClientMain.exe --server %SERVER% --port %PORT%

echo.
echo [停止] 客户端已停止。
pause

goto :eof

:show_help
echo ========================================
echo    PhysicsSync Client 启动帮助
echo ========================================
echo.
echo 用法: start_client.bat [选项]
echo.
echo 选项:
echo   --server ^<地址^>     服务器地址 (默认: localhost)
echo   --port ^<端口号^>     服务器端口 (默认: 9300)
echo   --build ^<类型^>      构建类型 (Debug/Release)
echo   --help              显示此帮助信息
echo.
echo 示例:
echo   start_client.bat --server 192.168.1.100
echo   start_client.bat --server localhost --port 8888
echo.
pause
exit /b 0

:build_project
    echo [构建] 正在创建构建目录...
    mkdir build 2>nul
    
    echo [构建] 正在配置CMake...
    cd build
    cmake -G "Visual Studio 17 2022" -D CMAKE_BUILD_TYPE=%BUILD_TYPE% ..
    if errorlevel 1 (
        echo [构建] 尝试使用默认生成器...
        cmake -D CMAKE_BUILD_TYPE=%BUILD_TYPE% ..
    )
    cd ..
    
    echo [构建] 正在编译...
    cmake --build build --config %BUILD_TYPE%
    
    if errorlevel 1 (
        echo [错误] 构建失败!
        exit /b 1
    )
    
    echo [构建] 构建成功!
    exit /b 0
