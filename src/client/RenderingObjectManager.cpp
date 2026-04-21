/**
 * @file RenderingObjectManager.cpp
 * @brief 渲染对象管理器实现
 *
 * 实现物理物体与渲染实体的映射，以及状态同步逻辑。
 * 使用 Filament 的 TransformManager 和 RenderableManager 管理渲染对象。
 */

#include "RenderingObjectManager.h"
#include <iostream>
#include <cmath>
#include <cstring>

namespace PhysicsSync {

// ================================================================
// 四元数到矩阵转换（Filament 使用 float[16] 行主序）
// ================================================================

/**
 * @brief 将四元数旋转转换为 4x4 变换矩阵
 * @param q 四元数
 * @param outMat4f 输出的 16 个浮点数（行主序）
 *
 * 矩阵格式（行主序）：
 * | m0  m4  m8  m12 |   | R00 R01 R02 Tx |
 * | m1  m5  m9  m13 | = | R10 R11 R12 Ty |
 * | m2  m6  m10 m14 |   | R20 R21 R22 Tz |
 * | m3  m7  m11 m15 |   |  0   0   0  1  |
 */
static void quatToMat4f(const Quat& q, float* outMat4f, const Vec3& pos) {
    // 四元数归一化
    float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    float ix, iy, iz, iw;
    if (len > 1e-8f) {
        ix = q.x / len; iy = q.y / len; iz = q.z / len; iw = q.w / len;
    } else {
        ix = iy = iz = 0.0f; iw = 1.0f;
    }

    // 旋转矩阵计算（行主序）
    float xx = ix * ix, yy = iy * iy, zz = iz * iz;
    float xy = ix * iy, xz = ix * iz, yz = iy * iz;
    float wx = iw * ix, wy = iw * iy, wz = iw * iz;

    // 行主序排列
    outMat4f[0]  = 1.0f - 2.0f * (yy + zz);   // R00
    outMat4f[1]  = 2.0f * (xy + wz);           // R01
    outMat4f[2]  = 2.0f * (xz - wy);           // R02
    outMat4f[3]  = 0.0f;                        // padding

    outMat4f[4]  = 2.0f * (xy - wz);           // R10
    outMat4f[5]  = 1.0f - 2.0f * (xx + zz);    // R11
    outMat4f[6]  = 2.0f * (yz + wx);           // R12
    outMat4f[7]  = 0.0f;                        // padding

    outMat4f[8]  = 2.0f * (xz + wy);           // R20
    outMat4f[9]  = 2.0f * (yz - wx);           // R21
    outMat4f[10] = 1.0f - 2.0f * (xx + yy);    // R22
    outMat4f[11] = 0.0f;                        // padding

    // 平移
    outMat4f[12] = pos.x;
    outMat4f[13] = pos.y;
    outMat4f[14] = pos.z;
    outMat4f[15] = 1.0f;
}

// ================================================================
// RenderingObjectManager 实现
// ================================================================

RenderingObjectManager::RenderingObjectManager(filament::Engine* engine,
                                                filament::Scene* scene)
    : engine_(engine), scene_(scene)
    , renderableMgr_(engine ? engine->getRenderableManager() : nullptr)
    , transformMgr_(engine ? engine->getTransformManager() : nullptr)
    , entityManager_(engine ? engine->getEntityManager() : nullptr)
{
}

RenderingObjectManager::~RenderingObjectManager() {
    destroy();
}

bool RenderingObjectManager::initialize() {
    if (!engine_ || !scene_) {
        std::cerr << "[RenderingObjectManager] Engine or scene is null!" << std::endl;
        return false;
    }

    if (!renderableMgr_ || !transformMgr_ || !entityManager_) {
        std::cerr << "[RenderingObjectManager] Manager pointers are null!" << std::endl;
        return false;
    }

    std::cout << "[RenderingObjectManager] Initialized." << std::endl;
    return true;
}

void RenderingObjectManager::destroy() {
    if (!engine_) return;

    // 销毁所有渲染对象
    for (auto& [objectId, data] : renderObjects_) {
        if (data.renderEntity != utils::Entity::INVALID() && scene_) {
            scene_->removeEntity(data.renderEntity);
        }
        if (data.renderable != filament::Camera::INVALID()) {
            renderableMgr_->destroy(data.renderable);
        }
        if (data.transformInstance) {
            transformMgr_->destroy(data.renderEntity);
        }
    }

    renderObjects_.clear();
    materialCache_.clear();

    std::cout << "[RenderingObjectManager] Destroyed. " << renderObjects_.size()
              << " objects remaining." << std::endl;
}

bool RenderingObjectManager::createObject(uint32_t objectId,
                                           const RenderObjectConfig& config) {
    if (!engine_ || !scene_) {
        std::cerr << "[RenderingObjectManager] Engine or scene is null!" << std::endl;
        return false;
    }

    // 检查是否已存在
    if (renderObjects_.find(objectId) != renderObjects_.end()) {
        std::cerr << "[RenderingObjectManager] Object " << objectId
                  << " already exists." << std::endl;
        return false;
    }

    // 创建 Filament 实体
    utils::Entity entity = entityManager_->create();
    if (entity == utils::Entity::INVALID()) {
        std::cerr << "[RenderingObjectManager] Failed to create entity for object "
                  << objectId << std::endl;
        return false;
    }

    // 创建变换组件
    transformMgr_->create(entity);

    // 计算包围盒半尺寸
    float halfSize = config.radius;
    float height = config.height;
    filament::Box boundingBox({-halfSize, -height * 0.5f, -halfSize},
                               { halfSize,  height * 0.5f,  halfSize});

    // 创建材质
    filament::MaterialInstance* matInst = createColorMaterial(
        config.material.r, config.material.g, config.material.b,
        config.material.emissive);

    // 构建渲染对象
    RenderObjectData& data = renderObjects_[objectId];
    data.renderEntity = entity;
    data.position = Vec3{0.0f, 0.0f, 0.0f};
    data.rotation = Quat::Identity();
    data.needsUpdate = true;

    // 添加到场景
    scene_->addEntity(entity);

    std::cout << "[RenderingObjectManager] Created object " << objectId
              << " (type=" << static_cast<int>(config.geometry) << ", radius="
              << config.radius << ")." << std::endl;

    return true;
}

bool RenderingObjectManager::destroyObject(uint32_t objectId) {
    auto it = renderObjects_.find(objectId);
    if (it == renderObjects_.end()) {
        return false;
    }

    RenderObjectData& data = it->second;

    if (data.renderEntity != utils::Entity::INVALID() && scene_) {
        scene_->removeEntity(data.renderEntity);
    }
    if (data.renderable != filament::Camera::INVALID()) {
        renderableMgr_->destroy(data.renderable);
    }

    // 清理材质缓存
    auto matIt = materialCache_.find(objectId);
    if (matIt != materialCache_.end()) {
        materialCache_.erase(matIt);
    }

    renderObjects_.erase(it);
    std::cout << "[RenderingObjectManager] Destroyed object " << objectId << "." << std::endl;
    return true;
}

bool RenderingObjectManager::hasObject(uint32_t objectId) const {
    return renderObjects_.find(objectId) != renderObjects_.end();
}

void RenderingObjectManager::updateTransform(uint32_t objectId,
                                              const Vec3& position,
                                              const Quat& rotation) {
    auto it = renderObjects_.find(objectId);
    if (it == renderObjects_.end()) {
        return; // 对象不存在，跳过
    }

    RenderObjectData& data = it->second;
    data.position = position;
    data.rotation = rotation;
    data.needsUpdate = true;
}

void RenderingObjectManager::syncFromSnapshot(const PhysicsWorldSnapshot& snapshot) {
    // 对于新对象，自动创建渲染对象
    for (const auto& obj : snapshot.objects) {
        RenderObjectConfig config;
        config.radius = 0.5f;
        config.height = 1.0f;

        // 根据对象类型设置不同颜色
        switch (obj.type) {
            case PhysicsObjectType::PLAYER:
                config.material = RenderMaterialConfig(0.0f, 1.0f, 0.0f); // 绿色 - 玩家
                break;
            case PhysicsObjectType::DYNAMIC:
                config.material = RenderMaterialConfig(0.5f, 0.5f, 1.0f); // 蓝色 - 动态物体
                break;
            case PhysicsObjectType::KINEMATIC:
                config.material = RenderMaterialConfig(1.0f, 1.0f, 0.0f); // 黄色 - 运动学
                break;
            case PhysicsObjectType::STATIC:
                config.material = RenderMaterialConfig(0.5f, 0.5f, 0.5f); // 灰色 - 静态
                break;
        }

        // 如果对象不存在，创建渲染对象
        if (!hasObject(obj.objectId)) {
            createObject(obj.objectId, config);
        }

        // 更新变换
        updateTransform(obj.objectId, obj.position, obj.rotation);
    }
}

void RenderingObjectManager::syncExistingFromSnapshot(
    const PhysicsWorldSnapshot& snapshot) {
    for (const auto& obj : snapshot.objects) {
        if (hasObject(obj.objectId)) {
            updateTransform(obj.objectId, obj.position, obj.rotation);
        }
    }
}

void RenderingObjectManager::applyUpdates() {
    if (!transformMgr_) return;

    for (auto& [objectId, data] : renderObjects_) {
        if (!data.needsUpdate) continue;
        if (data.renderEntity == utils::Entity::INVALID()) continue;

        // 将四元数和位置转换为变换矩阵
        float mat[16];
        quatToMat4f(data.rotation, mat, data.position);

        // 应用变换到 Filament
        auto instance = transformMgr_->getInstance(data.renderEntity);
        if (instance) {
            transformMgr_->setTransform(instance, mat);
        }

        data.needsUpdate = false;
    }
}

void RenderingObjectManager::setVisible(uint32_t objectId, bool visible) {
    auto it = renderObjects_.find(objectId);
    if (it == renderObjects_.end()) return;

    RenderObjectData& data = it->second;
    if (data.renderable != filament::Camera::INVALID()) {
        renderableMgr_->setVisibility(data.renderable, visible);
    }
}

Vec3 RenderingObjectManager::getPosition(uint32_t objectId) const {
    auto it = renderObjects_.find(objectId);
    if (it == renderObjects_.end()) {
        return Vec3::Zero();
    }
    return it->second.position;
}

bool RenderingObjectManager::createBoxGeometry(float width, float height,
                                                float depth) {
    // 立方体几何体在 Filament 中需要通过自定义顶点缓冲区创建
    // 此处为占位实现，实际使用时需要构建顶点数据
    (void)width; (void)height; (void)depth;
    return true;
}

bool RenderingObjectManager::createSphereGeometry(float radius, int segmentsU,
                                                   int segmentsV) {
    (void)radius; (void)segmentsU; (void)segmentsV;
    return true;
}

bool RenderingObjectManager::createPlaneGeometry(float width, float depth) {
    (void)width; (void)depth;
    return true;
}

filament::MaterialInstance* RenderingObjectManager::createColorMaterial(
    float r, float g, float b, bool emissive) {
    if (!engine_) return nullptr;

    // 创建一个简单的 PBR 材质
    // Filament 的材质需要预编译的 .matc 字节流
    // 此处使用简单的材质定义
    // 注意：在生产环境中，应使用预编译的材质文件

    // 创建一个基础材质实例
    // 使用 filament 内建材质作为基础
    // 由于内建材质需要 matc 编译，这里提供一个简化的实现

    // TODO: 在完整 Filament 集成中，使用 matc 编译的材质
    // 当前返回 nullptr，实际渲染需要预编译的材质文件

    return nullptr;
}

void RenderingObjectManager::setMaterialColor(uint32_t objectId,
                                               float r, float g, float b) {
    auto it = renderObjects_.find(objectId);
    if (it == renderObjects_.end()) return;
    (void)r; (void)g; (void)b;
    // 实际实现需要通过 MaterialInstance 设置参数
}

} // namespace PhysicsSync
