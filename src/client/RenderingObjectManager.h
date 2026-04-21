/**
 * @file RenderingObjectManager.h
 * @brief 渲染对象管理器 - 物理物体与渲染实体的映射
 *
 * 本文件定义 RenderingObjectManager 类，负责：
 * 1. 管理物理物体与渲染实体之间的映射关系
 * 2. 根据物理状态同步更新渲染实体的变换（位置/旋转）
 * 3. 创建和管理渲染几何体（使用 Filament 的 RenderableManager）
 * 4. 提供简单的几何体创建工具（立方体、球体等）
 *
 * 核心概念：
 * - ObjectId: 物理对象的唯一 ID
 * - RenderEntity: Filament 中的渲染实体 (utils::Entity)
 * - Transform: 由 TransformManager 管理的变换数据
 * - Renderable: 由 RenderableManager 管理的可渲染实体
 */

#pragma once

#include "../common/physics_state.h"
#include "FilamentRenderer.h"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

// 前置声明
namespace filament {
    class Engine;
    class RenderableManager;
    class TransformManager;
    class Material;
    class MaterialInstance;
    class Scene;
} // namespace filament

namespace utils {
    class EntityManager;
} // namespace utils

// 数学库前置声明
namespace math {
    class mat4f;
}

namespace PhysicsSync {

// ================================================================
// 渲染对象配置
// ================================================================

/**
 * @brief 渲染对象的材质配置
 */
struct RenderMaterialConfig {
    /// 材质颜色 (r,g,b 各 0.0-1.0)
    float r = 1.0f, g = 1.0f, b = 1.0f;
    /// 是否发光
    bool emissive = false;

    RenderMaterialConfig() = default;
    RenderMaterialConfig(float cr, float cg, float cb, bool emissive = false)
        : r(cr), g(cg), b(cb), emissive(emissive) {}
};

/**
 * @brief 渲染对象的几何体类型
 */
enum class GeometryType {
    BOX,        ///< 立方体
    SPHERE,     ///< 球体
    PLANE,      ///< 平面（地面）
    CYLINDER,   ///< 圆柱体
};

/**
 * @brief 渲染对象的配置
 */
struct RenderObjectConfig {
    /// 几何体类型
    GeometryType geometry = GeometryType::BOX;
    /// 半尺寸（对于 BOX/SPHERE）或半径（对于 CYLINDER）
    float radius = 0.5f;
    /// 高度（对于 CYLINDER/PLANE）
    float height = 1.0f;
    /// 材质配置
    RenderMaterialConfig material;
    /// 是否在物理世界中可见
    bool visible = true;

    RenderObjectConfig() = default;
};

// ================================================================
// 渲染对象内部数据
// ================================================================

/**
 * @brief 渲染对象内部状态
 *
 * 存储每个物理对象对应的渲染实体及其组件。
 */
struct RenderObjectData {
    /// Filament 渲染实体
    utils::Entity renderEntity = utils::Entity::INVALID();
    /// 渲染可组件实体
    filament::Camera renderable = filament::Camera::INVALID();
    /// 变换实例句柄
    void* transformInstance = nullptr;
    /// 当前物理位置
    Vec3 position;
    /// 当前物理旋转
    Quat rotation;
    /// 是否在下一帧需要更新
    bool needsUpdate = true;

    RenderObjectData() = default;
};

// ================================================================
// 渲染对象管理器
// ================================================================

/**
 * @brief 渲染对象管理器
 *
 * 核心职责：
 * 1. 为每个物理对象创建对应的 Filament 渲染实体
 * 2. 将物理状态同步到渲染对象的变换
 * 3. 管理材质和几何体
 *
 * 使用示例：
 * @code
 *   RenderingObjectManager manager(engine, scene);
 *
 *   // 创建一个渲染对象
 *   uint32_t objId = 1;
 *   RenderObjectConfig config;
 *   config.radius = 0.5f;
 *   config.material = RenderMaterialConfig(1.0f, 0.0f, 0.0f);
 *   manager.createObject(objId, config);
 *
 *   // 同步物理状态到渲染对象
 *   PhysicsObjectState state = ...; // 从服务器接收
 *   manager.updateTransform(objId, state.position, state.rotation);
 *
 *   // 每帧调用以应用变换更新
 *   manager.applyUpdates();
 * @endcode
 */
class RenderingObjectManager {
public:
    /**
     * @brief 构造函数
     * @param engine Filament 引擎指针
     * @param scene Filament 场景指针
     */
    explicit RenderingObjectManager(filament::Engine* engine,
                                    filament::Scene* scene);

    /// 析构函数
    ~RenderingObjectManager();

    // 禁止拷贝
    RenderingObjectManager(const RenderingObjectManager&) = delete;
    RenderingObjectManager& operator=(const RenderingObjectManager&) = delete;

    /**
     * @brief 初始化渲染管理器
     * @return 是否成功
     *
     * 创建共享材质和默认几何体缓冲区。
     */
    bool initialize();

    /**
     * @brief 销毁所有渲染资源
     */
    void destroy();

    // ================================================================
    // 对象创建与销毁
    // ================================================================

    /**
     * @brief 创建渲染对象
     * @param objectId 物理对象 ID
     * @param config 渲染配置
     * @return 是否成功创建
     */
    bool createObject(uint32_t objectId, const RenderObjectConfig& config);

    /**
     * @brief 销毁渲染对象
     * @param objectId 物理对象 ID
     * @return 是否成功销毁
     */
    bool destroyObject(uint32_t objectId);

    /**
     * @brief 检查渲染对象是否存在
     * @param objectId 物理对象 ID
     * @return 存在返回 true
     */
    bool hasObject(uint32_t objectId) const;

    /**
     * @brief 获取渲染对象数量
     */
    size_t getObjectCount() const { return renderObjects_.size(); }

    // ================================================================
    // 物理状态同步
    // ================================================================

    /**
     * @brief 更新单个对象的变换
     * @param objectId 物理对象 ID
     * @param position 新位置
     * @param rotation 新旋转
     *
     * 标记对象需要在下一帧应用变换更新。
     * Filament 的 TransformManager 会在应用时更新世界矩阵。
     */
    void updateTransform(uint32_t objectId,
                         const Vec3& position,
                         const Quat& rotation);

    /**
     * @brief 从世界快照批量同步状态
     * @param snapshot 物理世界快照
     *
     * 遍历快照中的所有对象，更新对应的渲染变换。
     * 对于新出现的对象，自动创建渲染对象。
     */
    void syncFromSnapshot(const PhysicsWorldSnapshot& snapshot);

    /**
     * @brief 从快照中只更新已有对象（不创建新对象）
     * @param snapshot 物理世界快照
     */
    void syncExistingFromSnapshot(const PhysicsWorldSnapshot& snapshot);

    /**
     * @brief 应用所有待处理的变换更新
     *
     * 调用此方法将标记为需要更新的对象的变换应用到 Filament。
     * 应在每帧渲染前调用。
     */
    void applyUpdates();

    /**
     * @brief 设置对象可见性
     * @param objectId 物理对象 ID
     * @param visible 是否可见
     */
    void setVisible(uint32_t objectId, bool visible);

    /**
     * @brief 获取对象的位置
     * @param objectId 物理对象 ID
     * @return 位置，对象不存在时返回零向量
     */
    Vec3 getPosition(uint32_t objectId) const;

    // ================================================================
    // 几何体创建（供高级用法）
    // ================================================================

    /**
     * @brief 创建立方体几何体
     * @param width 宽度
     * @param height 高度
     * @param depth 深度
     * @return 是否成功
     */
    bool createBoxGeometry(float width, float height, float depth);

    /**
     * @brief 创建球体几何体
     * @param radius 半径
     * @param segmentsU 水平分段数
     * @param segmentsV 垂直分段数
     * @return 是否成功
     */
    bool createSphereGeometry(float radius, int segmentsU = 16, int segmentsV = 12);

    /**
     * @brief 创建平面几何体
     * @param width 宽度
     * @param depth 深度
     * @return 是否成功
     */
    bool createPlaneGeometry(float width, float depth);

    // ================================================================
    // 材质创建（供高级用法）
    // ================================================================

    /**
     * @brief 创建简单颜色材质
     * @param r 红色分量 (0-1)
     * @param g 绿色分量 (0-1)
     * @param b 蓝色分量 (0-1)
     * @param emissive 是否发光材质
     * @return MaterialInstance 指针，失败返回 nullptr
     */
    filament::MaterialInstance* createColorMaterial(float r, float g, float b,
                                                     bool emissive = false);

    /**
     * @brief 设置对象的材质颜色
     * @param objectId 物理对象 ID
     * @param r 红色分量
     * @param g 绿色分量
     * @param b 蓝色分量
     */
    void setMaterialColor(uint32_t objectId, float r, float g, float b);

    // ================================================================
    // 查询接口
    // ================================================================

    /**
     * @brief 获取 Filament 引擎指针
     */
    filament::Engine* getEngine() const { return engine_; }

    /**
     * @brief 获取 Filament 场景指针
     */
    filament::Scene* getScene() const { return scene_; }

    /**
     * @brief 获取渲染对象映射表（供调试）
     */
    const std::unordered_map<uint32_t, RenderObjectData>& getRenderObjects() const {
        return renderObjects_;
    }

private:
    /**
     * @brief 根据配置创建几何体缓冲区
     * @param objectId 对象 ID
     * @param config 渲染配置
     * @return 顶点缓冲区数量
     */
    uint32_t createGeometryBuffers(uint32_t objectId, const RenderObjectConfig& config);

    /**
     * @brief 将四元数转换为 Filament 使用的 mat4f 变换矩阵
     * @param position 位置
     * @param rotation 旋转
     * @return 4x4 变换矩阵
     */
    static void rotationToMatrix(const Quat& q, void* outMat4f);

    // -- Filament 管理器 --
    filament::Engine* engine_ = nullptr;       ///< Filament 引擎
    filament::Scene* scene_ = nullptr;         ///< 场景
    filament::RenderableManager* renderableMgr_ = nullptr; ///< 可渲染管理器
    filament::TransformManager* transformMgr_ = nullptr;   ///< 变换管理器
    utils::EntityManager* entityManager_ = nullptr;        ///< 实体管理器

    // -- 渲染对象映射: objectId -> RenderObjectData --
    std::unordered_map<uint32_t, RenderObjectData> renderObjects_;

    // -- 几何体缓存: geometryKey -> (vertexBuffer, indexBuffer, primitiveCount) --
    // geometryKey = (type, width, height, depth)
    struct GeometryKey {
        GeometryType type;
        float radius, height, depth;
        bool operator==(const GeometryKey& other) const {
            return type == other.type &&
                   std::abs(radius - other.radius) < 0.001f &&
                   std::abs(height - other.height) < 0.001f &&
                   std::abs(depth - other.depth) < 0.001f;
        }
    };

    // -- 材质缓存: objectId -> MaterialInstance --
    std::unordered_map<uint32_t, filament::MaterialInstance*> materialCache_;
};

} // namespace PhysicsSync
