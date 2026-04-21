/**
 * @file FilamentRenderer.h
 * @brief Filament 渲染引擎封装 - 基于 Google Filament 的实时渲染系统
 *
 * 本文件定义 FilamentRenderer 类，负责：
 * 1. 初始化 Google Filament 渲染引擎
 * 2. 管理渲染器、场景、视图的生命周期
 * 3. 处理窗口/交换链（通过原生句柄回调）
 * 4. 提供渲染帧的触发接口
 *
 * 设计原则：
 * - 抽象底层窗口系统，通过原生句柄回调适配不同平台
 * - 提供简化的 API 供 PhysicsClient 调用
 * - 支持离屏渲染和窗口渲染两种模式
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <memory>

// 前置声明：避免在头文件中包含 Filament 头文件，减少编译依赖
namespace filament {
    class Engine;
    class Renderer;
    class Scene;
    class View;
    class SwapChain;
    class Camera;
} // namespace filament

namespace utils {
    class EntityManager;
} // namespace utils

namespace PhysicsSync {

// ================================================================
// 渲染配置
// ================================================================

/**
 * @brief 渲染器配置结构体
 *
 * 用于配置 Filament 渲染器的各种参数。
 */
struct RendererConfig {
    /// 窗口宽度（像素），离屏模式默认 1280
    int32_t width = 1280;
    /// 窗口高度（像素），离屏模式默认 720
    int32_t height = 720;
    /// 是否为窗口模式（true=窗口，false=离屏渲染）
    bool windowed = false;
    /// 窗口标题（仅窗口模式有效）
    std::string windowTitle = "PhysicsSync Client";
    /// 垂直同步开关
    bool vsync = false;

    RendererConfig() = default;
    RendererConfig(int32_t w, int32_t h, bool windowed = false)
        : width(w), height(h), windowed(windowed) {}
};

// ================================================================
// 原生窗口句柄回调
// ================================================================

/**
 * @brief 获取原生窗口句柄的回调函数
 *
 * 由于 Filament 需要特定平台的窗口句柄来创建 SwapChain，
 * 我们通过回调让上层提供窗口句柄。
 *
 * @return 原生窗口句柄（Windows 上为 HWND）
 */
using NativeWindowHandleCallback = std::function<void*>();

// ================================================================
// Filament 渲染引擎封装
// ================================================================

/**
 * @brief Google Filament 渲染引擎封装
 *
 * 本类封装了 Filament 的核心组件：
 * - Engine: 创建和管理所有其他 Filament 对象
 * - Renderer: 实际的渲染器（对应一个窗口）
 * - Scene: 场景图（包含要渲染的实体和灯光）
 * - View: 视图定义（相机的视锥和渲染目标）
 * - SwapChain: 交换链（窗口表面）
 *
 * 用法示例：
 * @code
 *   FilamentRenderer renderer;
 *   RendererConfig config;
 *   config.width = 1280;
 *   config.height = 720;
 *   renderer.init(config, getNativeWindowHandle);
 *   // 创建场景、相机、物体...
 *   while (running) {
 *       renderer.beginFrame();
 *       renderer.render();
 *       renderer.endFrame();
 *   }
 *   renderer.destroy();
 * @endcode
 */
class FilamentRenderer {
public:
    /// 构造函数
    FilamentRenderer();

    /// 析构函数
    ~FilamentRenderer();

    // 禁止拷贝
    FilamentRenderer(const FilamentRenderer&) = delete;
    FilamentRenderer& operator=(const FilamentRenderer&) = delete;

    /**
     * @brief 初始化渲染引擎
     * @param config 渲染配置
     * @param getWindowHandle 获取原生窗口句柄的回调（窗口模式需要）
     * @return 是否成功初始化
     */
    bool init(const RendererConfig& config,
              NativeWindowHandleCallback getWindowHandle);

    /**
     * @brief 销毁渲染资源
     *
     * 释放所有 Filament 对象，必须在程序退出前调用。
     */
    void destroy();

    // ================================================================
    // 场景管理
    // ================================================================

    /**
     * @brief 创建场景
     * @param name 场景名称（用于调试）
     * @return 是否成功创建
     */
    bool createScene(const char* name = "MainScene");

    /**
     * @brief 销毁场景
     */
    void destroyScene();

    /**
     * @brief 获取场景指针
     * @return 场景指针，未创建时返回 nullptr
     */
    filament::Scene* getScene() const { return scene_; }

    /**
     * @brief 设置场景的太阳光强度
     * @param intensity 光强度
     */
    void setSunIntensity(float intensity);

    // ================================================================
    // 相机管理
    // ================================================================

    /**
     * @brief 创建相机
     * @param aspectRatio 宽高比
     * @param nearPlane 近裁剪面距离
     * @param farPlane 远裁剪面距离
     * @return 是否成功创建
     *
     * 使用 Filament 内部的 EntityManager 创建相机实体。
     */
    bool createCamera(float aspectRatio = 16.0f / 9.0f,
                      float nearPlane = 0.01f,
                      float farPlane = 1000.0f);

    /**
     * @brief 销毁相机
     */
    void destroyCamera();

    /**
     * @brief 获取相机实体
     * @return 相机实体，未创建时返回无效实体
     */
    filament::Camera getCamera() const;

    /**
     * @brief 设置相机位置（世界坐标）
     * @param x, y, z 坐标
     */
    void setCameraPosition(float x, float y, float z);

    /**
     * @brief 设置相机注视点（世界坐标）
     * @param x, y, z 注视点坐标
     */
    void setCameraLookAt(float x, float y, float z);

    // ================================================================
    // 渲染循环
    // ================================================================

    /**
     * @brief 开始渲染帧（设置帧缓冲）
     * @param w 帧缓冲宽度
     * @param h 帧缓冲高度
     * @return 是否成功
     */
    bool beginFrame(int32_t w, int32_t h);

    /**
     * @brief 执行单次渲染
     * @return 是否成功渲染
     *
     * 同步渲染调用，会等待 GPU 完成。
     * 生产环境应使用异步渲染。
     */
    bool render();

    /**
     * @brief 结束当前帧（呈现）
     * @return 是否成功
     */
    bool endFrame();

    /**
     * @brief 渲染一帧（begin + render + end 组合）
     * @param w 帧缓冲宽度
     * @param h 帧缓冲高度
     * @return 是否成功
     */
    bool renderFrame(int32_t w, int32_t h);

    // ================================================================
    // 查询接口
    // ================================================================

    /**
     * @brief 获取引擎指针
     * @return 引擎指针
     */
    filament::Engine* getEngine() const { return engine_; }

    /**
     * @brief 获取渲染器指针
     * @return 渲染器指针
     */
    filament::Renderer* getRenderer() const { return renderer_; }

    /**
     * @brief 获取视图指针
     * @return 视图指针
     */
    filament::View* getView() const { return view_; }

    /**
     * @brief 检查渲染器是否已初始化
     * @return 已初始化返回 true
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief 获取配置
     */
    const RendererConfig& getConfig() const { return config_; }

private:
    /**
     * @brief 创建 SwapChain（仅窗口模式）
     * @return 是否成功
     */
    bool createSwapChain();

    /**
     * @brief 销毁 SwapChain
     */
    void destroySwapChain();

    /**
     * @brief 创建视图
     */
    void createView();

    // -- Filament 核心对象 --
    filament::Engine*    engine_    = nullptr; ///< 引擎（必须最先创建，最后销毁）
    filament::Renderer* renderer_   = nullptr; ///< 渲染器
    filament::Scene*    scene_      = nullptr; ///< 场景
    filament::View*     view_       = nullptr; ///< 视图
    filament::Camera    camera_;               ///< 相机实体
    filament::SwapChain swapChain_  = nullptr; ///< 交换链（窗口表面）

    // -- 配置与状态 --
    RendererConfig config_;                ///< 渲染配置
    NativeWindowHandleCallback getWindowHandle_; ///< 窗口句柄回调
    bool initialized_ = false;             ///< 是否已初始化
    bool sceneCreated_ = false;            ///< 场景是否已创建
    bool cameraCreated_ = false;           ///< 相机是否已创建
};

} // namespace PhysicsSync
