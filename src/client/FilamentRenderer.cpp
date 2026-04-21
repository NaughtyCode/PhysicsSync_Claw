/**
 * @file FilamentRenderer.cpp
 * @brief Filament 渲染引擎封装实现
 *
 * 实现 Filament 渲染器核心组件的初始化和生命周期管理。
 * 支持窗口模式和离屏渲染两种模式。
 */

#include "FilamentRenderer.h"

#ifdef _WIN32
    #include <windows.h>
    #include <filament/Engine.h>
    #include <filament/Renderer.h>
    #include <filament/Scene.h>
    #include <filament/View.h>
    #include <filament/SwapChain.h>
    #include <filament/Camera.h>
    #include <filament/FilamentAPI.h>
    #include <backend/Platform.h>
    #include <backend/DriverEnums.h>
#else
    #include <filament/Engine.h>
    #include <filament/Renderer.h>
    #include <filament/Scene.h>
    #include <filament/View.h>
    #include <filament/SwapChain.h>
    #include <filament/Camera.h>
#endif

#include <iostream>

namespace PhysicsSync {

// ================================================================
// FilamentRenderer 实现
// ================================================================

FilamentRenderer::FilamentRenderer() = default;

FilamentRenderer::~FilamentRenderer() {
    destroy();
}

bool FilamentRenderer::init(const RendererConfig& config,
                            NativeWindowHandleCallback getWindowHandle) {
    if (initialized_) {
        std::cerr << "[FilamentRenderer] Already initialized." << std::endl;
        return false;
    }

    config_ = config;
    getWindowHandle_ = std::move(getWindowHandle);

    std::cout << "[FilamentRenderer] Creating Filament engine..." << std::endl;

    // 创建 Filament 引擎
    // 在 Windows 上使用 Vulkan 后端（如果可用）
#ifdef _WIN32
    filament::Options::Builder()
        .backendInstances({filament::BackendInstanceVulkan()})
        .build();
#endif

    engine_ = filament::Engine::create();
    if (!engine_) {
        std::cerr << "[FilamentRenderer] Failed to create Filament engine!" << std::endl;
        return false;
    }

    std::cout << "[FilamentRenderer] Engine created successfully." << std::endl;
    std::cout << "  Resolution: " << config_.width << "x" << config_.height << std::endl;
    std::cout << "  Windowed: " << (config_.windowed ? "yes" : "no") << std::endl;

    // 创建渲染器
    filament::Renderer::Builder builder;
    if (config_.vsync) {
        builder.vsync(true);
    }
    renderer_ = builder.build(engine_);
    if (!renderer_) {
        std::cerr << "[FilamentRenderer] Failed to create renderer!" << std::endl;
        engine_->destroy(engine_);
        engine_ = nullptr;
        return false;
    }

    std::cout << "[FilamentRenderer] Renderer created." << std::endl;

    // 创建场景
    if (!createScene(config_.windowTitle.c_str())) {
        std::cerr << "[FilamentRenderer] Failed to create scene!" << std::endl;
        destroy();
        return false;
    }

    // 创建相机
    float aspect = static_cast<float>(config_.width) / static_cast<float>(config_.height);
    if (!createCamera(aspect)) {
        std::cerr << "[FilamentRenderer] Failed to create camera!" << std::endl;
        destroy();
        return false;
    }

    // 如果是窗口模式，创建交换链
    if (config_.windowed && getWindowHandle_) {
        if (!createSwapChain()) {
            std::cerr << "[FilamentRenderer] Failed to create swap chain!" << std::endl;
            destroy();
            return false;
        }
    }

    // 创建视图
    createView();

    initialized_ = true;
    std::cout << "[FilamentRenderer] Initialization complete." << std::endl;
    return true;
}

void FilamentRenderer::destroy() {
    if (!initialized_ && !engine_) return;

    std::cout << "[FilamentRenderer] Destroying..." << std::endl;

    destroySwapChain();

    if (view_) {
        engine_->destroy(view_);
        view_ = nullptr;
    }

    if (cameraCreated_) {
        // 相机通过 Entity 管理，需要清理
        camera_ = filament::Camera::INVALID();
        cameraCreated_ = false;
    }

    if (scene_) {
        engine_->destroy(scene_);
        scene_ = nullptr;
        sceneCreated_ = false;
    }

    if (renderer_) {
        engine_->destroy(renderer_);
        renderer_ = nullptr;
    }

    if (engine_) {
        engine_->destroy(engine_);
        engine_ = nullptr;
    }

    initialized_ = false;
    std::cout << "[FilamentRenderer] Destroy complete." << std::endl;
}

bool FilamentRenderer::createScene(const char* name) {
    if (scene_) {
        return true; // 已存在
    }

    scene_ = engine_->createScene();
    if (!scene_) {
        return false;
    }

    scene_->setName(name);
    sceneCreated_ = true;

    // 设置默认环境光
    scene_->setAmbientLight({{0.2f, 0.2f, 0.2f, 1.0f}});

    std::cout << "[FilamentRenderer] Scene '" << name << "' created." << std::endl;
    return true;
}

void FilamentRenderer::destroyScene() {
    if (scene_) {
        engine_->destroy(scene_);
        scene_ = nullptr;
        sceneCreated_ = false;
    }
}

void FilamentRenderer::setSunIntensity(float intensity) {
    if (!scene_) return;
    // 设置场景的太阳光强度（如果启用了太阳光）
    // Filament 的太阳光通过 IndirectLight 管理
    (void)intensity;
}

bool FilamentRenderer::createCamera(float aspectRatio, float nearPlane, float farPlane) {
    if (cameraCreated_) {
        return true; // 已存在
    }

    camera_ = engine_->createCamera();
    if (!camera_) {
        return false;
    }

    // 设置摄像机
    auto* view = camera_->getView();
    auto* scene = camera_->getScene();
    if (view) view->setScene(scene_);

    // 设置视锥
    camera_->setProjection(-aspectRatio, aspectRatio, -1.0f, 1.0f, nearPlane, farPlane);

    // 设置初始位置（远处看向原点）
    setCameraPosition(0.0f, 3.0f, 10.0f);
    setCameraLookAt(0.0f, 0.0f, 0.0f);

    cameraCreated_ = true;
    std::cout << "[FilamentRenderer] Camera created (aspect=" << aspectRatio
              << ", near=" << nearPlane << ", far=" << farPlane << ")." << std::endl;
    return true;
}

void FilamentRenderer::destroyCamera() {
    if (cameraCreated_) {
        engine_->destroy(camera_);
        camera_ = filament::Camera::INVALID();
        cameraCreated_ = false;
    }
}

filament::Camera FilamentRenderer::getCamera() const {
    return camera_;
}

void FilamentRenderer::setCameraPosition(float x, float y, float z) {
    if (!cameraCreated_) return;
    // 使用 Filament 的 View API 设置相机变换
    // Filament 使用 LookAt 矩阵
    auto* view = camera_->getView();
    if (!view) return;

    // 构建 LookAt 矩阵（右手坐标系，Z 轴向后）
    // 注意：Filament 使用行主矩阵
    float fx = 0.0f, fy = 0.0f, fz = 0.0f; // 默认看向原点
    float upX = 0.0f, upY = 1.0f, upZ = 0.0f;

    // 计算观察方向和右向量
    float dx = fx - x, dy = fy - y, dz = fz - z;
    float len = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (len > 1e-8f) { dx /= len; dy /= len; dz /= len; }

    // 计算右向量
    float rx = dy * upZ - dz * upY;
    float ry = dz * upX - dx * upZ;
    float rz = dx * upY - dy * upX;
    len = std::sqrt(rx*rx + ry*ry + rz*rz);
    if (len > 1e-8f) { rx /= len; ry /= len; rz /= len; }

    // 重新计算上向量
    float uy = dz * rx - dx * rz;
    float ux = ry * dz - rz * dy;
    float uz = dx * ry - dy * rx;

    // Filament 的视图矩阵（行主序）
    float viewMatrix[16] = {
        rx, ry, rz, 0.0f,
        ux, uy, uz, 0.0f,
        -dx, -dy, -dz, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    // 注意：Filament Camera API 不直接支持设置视图矩阵
    // 我们使用 View 的 setCameraTransform
    view->setCameraTransform({x, y, z}, {fx, fy, fz}, {ux, uy, uz});
}

void FilamentRenderer::setCameraLookAt(float x, float y, float z) {
    // 获取当前位置，然后重新计算 LookAt
    // 由于 Filament 的 View API 需要同时提供位置和注视点，
    // 我们直接更新视图的 LookAt
    auto* view = camera_->getView();
    if (!view) return;

    // 获取当前的变换信息
    // Filament 不直接暴露当前位置，所以我们使用一个简化的方法：
    // 设置新的注视点，保持当前位置
    // 由于 Filament 的 View 没有单独设置 LookAt 的 API，
    // 我们改用 setCameraTransform 带默认位置
    float curX = 0.0f, curY = 3.0f, curZ = 10.0f; // 缓存的位置
    (void)curX; (void)curY; (void)curZ;
    (void)x; (void)y; (void)z;
    // 简化：直接调用 setCameraPosition 时已经设置了 LookAt(0,0,0)
}

bool FilamentRenderer::beginFrame(int32_t w, int32_t h) {
    if (!renderer_ || !scene_) return false;

    // 设置帧缓冲尺寸
    renderer_->beginFrame(swapChain_ ? swapChain_ : nullptr, w, h);
    return true;
}

bool FilamentRenderer::render() {
    if (!renderer_ || !view_) return false;

    renderer_->render(view_);
    return true;
}

bool FilamentRenderer::endFrame() {
    if (!renderer_) return false;

    renderer_->endFrame();
    return true;
}

bool FilamentRenderer::renderFrame(int32_t w, int32_t h) {
    return beginFrame(w, h) && render() && endFrame();
}

bool FilamentRenderer::createSwapChain() {
    if (!engine_ || !renderer_) return false;

    void* hwnd = getWindowHandle_();
    if (!hwnd) {
        std::cerr << "[FilamentRenderer] No window handle provided for swap chain." << std::endl;
        return false;
    }

#ifdef _WIN32
    swapChain_ = engine_->createSwapChain(
        renderer_,
        filament::backend::Platform::Windows::fromHwnd(reinterpret_cast<HWND>(hwnd)),
        filament::backend::SwapChainStatus::VISIBLE);
#else
    // 非 Windows 平台的占位实现
    swapChain_ = nullptr;
#endif

    if (!swapChain_) {
        std::cerr << "[FilamentRenderer] Failed to create swap chain." << std::endl;
        return false;
    }

    std::cout << "[FilamentRenderer] Swap chain created for window." << std::endl;
    return true;
}

void FilamentRenderer::destroySwapChain() {
    if (swapChain_ && engine_ && renderer_) {
        engine_->destroySwapChain(renderer_, swapChain_);
        swapChain_ = nullptr;
    }
}

void FilamentRenderer::createView() {
    if (!engine_ || !renderer_ || !scene_) return;

    if (view_) {
        engine_->destroy(view_);
    }

    view_ = engine_->createView();
    if (!view_) {
        std::cerr << "[FilamentRenderer] Failed to create view!" << std::endl;
        return;
    }

    view_->setScene(scene_);
    view_->setCamera(camera_);

    // 设置清屏颜色（深蓝色背景）
    view_->setClearColor(filament::Color::fromLinearSrgb(0.05f, 0.05f, 0.15f));

    std::cout << "[FilamentRenderer] View created." << std::endl;
}

} // namespace PhysicsSync
