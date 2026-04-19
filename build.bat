@echo off
REM === ==================================================
REM PhysicsSync 构建脚本 (Windows)
REM === ==================================================
REM 
REM 使用方法:
REM   build.bat [选项]
REM 
REM 选项:
REM   --clean             清理构建目录
REM   --build ^<类型^>      构建类型 (Debug/Release/RelWithDebInfo)
REM   --test              构建并运行测试
REM   --help              显示帮助信息
REM 
REM === ==================================================

setlocal enabledelayedexpansion

REM 默认参数
set BUILD_TYPE=Release
set RUN_TEST=0
set DO_CLEAN=0

REM 解析命令行参数
:parse_args
if "%~1"=="" goto :end_parse
if "%~1"=="--clean" (
    set DO_CLEAN=1
    shift
    goto :parse_args
)
if "%~1"=="--build" (
    set BUILD_TYPE=%~2
    shift /2
    goto :parse_args
)
if "%~1"=="--test" (
    set RUN_TEST=1
    shift
    goto :parse_args
)
if "%~1"=="--help" (
    goto :show_help
)
shift
goto :parse_args

:end_parse

echo ========================================
echo    PhysicsSync Build Script
echo ========================================
echo.
echo  构建类型: %BUILD_TYPE%
echo  清理构建: %DO_CLEAN%
echo  运行测试: %RUN_TEST%
echo.

REM 清理构建目录
if %DO_CLEAN%==1 (
    echo [清理] 正在清理构建目录...
    if exist "build" (
        rmdir /s /q build
        echo [清理] 构建目录已删除。
    )
)

REM 创建构建目录
echo [构建] 正在创建构建目录...
mkdir build 2>nul
cd build

REM 配置CMake
echo [构建] 正在配置CMake...
cmake -G "Visual Studio 17 2022" -A x64 -D CMAKE_BUILD_TYPE=%BUILD_TYPE% ..
if errorlevel 1 (
    echo [构建] 尝试使用默认生成器...
    cmake -D CMAKE_BUILD_TYPE=%BUILD_TYPE% ..
    if errorlevel 1 (
        echo [错误] CMake配置失败!
        cd ..
        pause
        exit /b 1
    )
)

REM 编译
echo.
echo [构建] 正在编译...
cmake --build . --config %BUILD_TYPE% --parallel
if errorlevel 1 (
    echo [错误] 编译失败!
    cd ..
    pause
    exit /b 1
)

echo.
echo [构建] 构建成功!

REM 运行测试
if %RUN_TEST%==1 (
    echo.
    echo [测试] 正在运行测试...
    ctest -C %BUILD_TYPE% --output-on-failure
    if errorlevel 1 (
        echo [警告] 部分测试失败!
    ) else (
        echo [测试] 所有测试通过!
    )
)

cd ..
echo.
echo [完成] 构建完成。
pause

goto :eof

:show_help
echo ========================================
echo    PhysicsSync 构建帮助
echo ========================================
echo.
echo 用法: build.bat [选项]
echo.
echo 选项:
echo   --clean             清理构建目录
echo   --build ^<类型^>      构建类型 (Debug/Release/RelWithDebInfo)
echo   --test              构建并运行测试
echo   --help              显示此帮助信息
echo.
echo 示例:
echo   build.bat --build Debug
echo   build.bat --clean --build Release --test
echo.
pause
exit /b 0
