@echo off
REM === ==================================================
REM PhysicsSync 服务器启动脚本 (Windows)
REM === ==================================================
REM 
REM 使用方法:
REM   start_server.bat [选项]
REM 
REM 选项:
REM   --port <端口号>       监听端口 (默认: 7777)
REM   --physics-hz <频率>   物理更新频率 (默认: 60)
REM   --snapshot-hz <频率>  快照广播频率 (默认: 30)
REM   --help               显示帮助信息
REM 
REM === ==================================================

setlocal enabledelayedexpansion

REM 默认参数
set PORT=9300
set PHYSICS_HZ=60
set SNAPSHOT_HZ=30
set BUILD_TYPE=Release

REM 解析命令行参数
:parse_args
if "%~1"=="" goto :end_parse
if "%~1"=="--port" (
    set PORT=%~2
    shift /2
    goto :parse_args
)
if "%~1"=="--physics-hz" (
    set PHYSICS_HZ=%~2
    shift /2
    goto :parse_args
)
if "%~1"=="--snapshot-hz" (
    set SNAPSHOT_HZ=%~2
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
echo    PhysicsSync Server (Windows)
echo ========================================
echo.
echo  配置参数:
echo    端口:          %PORT%
echo    物理频率:      %PHYSICS_HZ% Hz
echo    快照频率:      %SNAPSHOT_HZ% Hz
echo    构建类型:      %BUILD_TYPE%
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
if not exist "build\bin\%BUILD_TYPE%\PhysicsSyncServerMain.exe" (
    echo [错误] 找不到服务器可执行文件!
    echo [提示] 请先运行: build.bat
    pause
    exit /b 1
)

echo [启动] 正在启动服务器...
echo.

REM 启动服务器
build\bin\%BUILD_TYPE%\PhysicsSyncServerMain.exe --port %PORT% --physics-hz %PHYSICS_HZ% --snapshot-hz %SNAPSHOT_HZ%

echo.
echo [停止] 服务器已停止。
pause

goto :eof

:show_help
echo ========================================
echo    PhysicsSync Server 启动帮助
echo ========================================
echo.
echo 用法: start_server.bat [选项]
echo.
echo 选项:
echo   --port ^<端口号^>       监听端口 (默认: 9300)
echo   --physics-hz ^<频率^>   物理更新频率 (默认: 60)
echo   --snapshot-hz ^<频率^>  快照广播频率 (默认: 30)
echo   --build ^<类型^>        构建类型 (Debug/Release)
echo   --help               显示此帮助信息
echo.
echo 示例:
echo   start_server.bat --port 8888
echo   start_server.bat --physics-hz 120 --snapshot-hz 60
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
