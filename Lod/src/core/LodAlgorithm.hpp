#pragma once

#include "Mesh.hpp"
#include "Geometry.hpp"
#include "../geo/GeoBBox.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <optional>
#include <variant>

namespace lod::core {

// 地理 LOD 节点结构
struct GeoLodNode {
    geo::GeoBBox region;
    std::vector<std::shared_ptr<GeoLodNode>> children;
    Mesh mesh;
    int lodLevel{0};
    double geometricError{0.0};
    
    // 查询方法
    bool isLeaf() const noexcept { return children.empty(); }
    size_t childCount() const noexcept { return children.size(); }
    
    // 遍历器
    template<typename Visitor>
    void traverse(Visitor&& visitor) const {
        visitor(*this);
        for (const auto& child : children) {
            child->traverse(std::forward<Visitor>(visitor));
        }
    }
};

// LOD 节点的变体类型
using LodNode = std::variant<GeoLodNode, GeometricLodNode>;

// LOD 简化策略接口
class ILodStrategy {
public:
    virtual ~ILodStrategy() = default;
    
    // 计算目标面片数
    virtual size_t targetTriangleCount(const Mesh& mesh, int lodLevel) const = 0;
    
    // 计算几何误差
    virtual double computeGeometricError(const Mesh& original, const Mesh& simplified) const = 0;
    
    // 判断是否需要进一步细分（地理模式）
    virtual bool shouldSubdivide(const Mesh& mesh, const geo::GeoBBox& region, int currentLevel) const = 0;
    
    // 判断是否需要进一步细分（几何模式）
    virtual bool shouldSubdivide(const Mesh& mesh, const BoundingBox& bounds, int currentLevel) const = 0;
};

// 基于面片数的策略
class TriangleCountStrategy : public ILodStrategy {
public:
    explicit TriangleCountStrategy(size_t maxTrianglesPerTile = 50000, 
                                  double reductionRatio = 0.5)
        : maxTrianglesPerTile_(maxTrianglesPerTile), reductionRatio_(reductionRatio) {}
    
    size_t targetTriangleCount(const Mesh& mesh, int lodLevel) const override;
    double computeGeometricError(const Mesh& original, const Mesh& simplified) const override;
    bool shouldSubdivide(const Mesh& mesh, const geo::GeoBBox& region, int currentLevel) const override;
    bool shouldSubdivide(const Mesh& mesh, const BoundingBox& bounds, int currentLevel) const override;
    
private:
    size_t maxTrianglesPerTile_;
    double reductionRatio_;
};

// 基于屏幕空间误差的策略
class ScreenSpaceErrorStrategy : public ILodStrategy {
public:
    explicit ScreenSpaceErrorStrategy(double maxScreenSpaceError = 16.0)
        : maxScreenSpaceError_(maxScreenSpaceError) {}
    
    size_t targetTriangleCount(const Mesh& mesh, int lodLevel) const override;
    double computeGeometricError(const Mesh& original, const Mesh& simplified) const override;
    bool shouldSubdivide(const Mesh& mesh, const geo::GeoBBox& region, int currentLevel) const override;
    bool shouldSubdivide(const Mesh& mesh, const BoundingBox& bounds, int currentLevel) const override;
    
private:
    double maxScreenSpaceError_;
};

// 基于体积的策略（几何模式专用）
class VolumeBasedStrategy : public ILodStrategy {
public:
    explicit VolumeBasedStrategy(float minVolumeThreshold = 0.001f,
                                double reductionRatio = 0.5)
        : minVolumeThreshold_(minVolumeThreshold), reductionRatio_(reductionRatio) {}
    
    size_t targetTriangleCount(const Mesh& mesh, int lodLevel) const override;
    double computeGeometricError(const Mesh& original, const Mesh& simplified) const override;
    bool shouldSubdivide(const Mesh& mesh, const geo::GeoBBox& region, int currentLevel) const override;
    bool shouldSubdivide(const Mesh& mesh, const BoundingBox& bounds, int currentLevel) const override;
    
private:
    float minVolumeThreshold_;
    double reductionRatio_;
};

// LOD 生成配置
struct LodConfig {
    std::unique_ptr<ILodStrategy> strategy;
    int maxLodLevels{8};
    size_t minTrianglesForSubdivision{100};
    
    // 地理模式配置
    double minTileSizeDegrees{0.001};  // 最小瓦片尺寸（度）
    
    // 几何模式配置
    float minNodeSize{0.001f};         // 最小节点尺寸
    
    // 八叉树配置
    OctreeConfig octreeConfig;
    
    // 通用配置
    bool enableParallelProcessing{true};
    bool useOctreeSubdivision{true};   // 是否使用八叉树细分
};

// LOD 构建模式
enum class LodMode {
    Geographic,  // 地理模式（使用四叉树）
    Geometric    // 几何模式（使用八叉树）
};

// 纯函数：网格简化
[[nodiscard]] Mesh simplifyMesh(const Mesh& mesh, size_t targetTriangleCount) noexcept;

// 纯函数：根据地理区域分割网格
[[nodiscard]] std::vector<std::pair<Mesh, geo::GeoBBox>> 
splitMeshByRegion(const Mesh& mesh, const geo::GeoBBox& totalRegion, 
                  const std::vector<geo::GeoBBox>& subRegions);

// 纯函数：根据几何包围盒分割网格
[[nodiscard]] std::vector<std::pair<Mesh, BoundingBox>>
splitMeshByBounds(const Mesh& mesh, const BoundingBox& totalBounds,
                  const std::vector<BoundingBox>& subBounds);

// 纯函数：构建地理 LOD 层次结构
[[nodiscard]] std::shared_ptr<GeoLodNode> 
buildGeoLodHierarchy(const Mesh& inputMesh, const geo::GeoBBox& region, 
                     const LodConfig& config);

// 纯函数：构建几何 LOD 层次结构
[[nodiscard]] std::shared_ptr<GeometricLodNode>
buildGeometricLodHierarchy(const Mesh& inputMesh, const BoundingBox& bounds,
                          const LodConfig& config);

// 纯函数：通用 LOD 构建（自动选择模式）
[[nodiscard]] LodNode
buildLodHierarchy(const Mesh& inputMesh, 
                  const std::variant<geo::GeoBBox, BoundingBox>& bounds,
                  const LodConfig& config);

// 纯函数：使用八叉树构建几何 LOD
[[nodiscard]] std::shared_ptr<GeometricLodNode>
buildOctreeLodHierarchy(const Mesh& inputMesh, const LodConfig& config);

// 纯函数：合并相邻的地理 LOD 节点
[[nodiscard]] std::shared_ptr<GeoLodNode> 
mergeGeoLodNodes(std::span<const std::shared_ptr<GeoLodNode>> nodes);

// 纯函数：合并相邻的几何 LOD 节点
[[nodiscard]] std::shared_ptr<GeometricLodNode>
mergeGeometricLodNodes(std::span<const std::shared_ptr<GeometricLodNode>> nodes);

// 纯函数：计算地理 LOD 节点统计信息
struct GeoLodStats {
    size_t totalNodes{0};
    size_t leafNodes{0};
    size_t totalTriangles{0};
    int maxDepth{0};
    std::vector<size_t> trianglesPerLevel;
    geo::GeoBBox totalRegion;
};

[[nodiscard]] GeoLodStats computeGeoLodStats(const GeoLodNode& root) noexcept;

// 纯函数：计算几何 LOD 节点统计信息
struct GeometricLodStats {
    size_t totalNodes{0};
    size_t leafNodes{0};
    size_t totalTriangles{0};
    int maxDepth{0};
    std::vector<size_t> trianglesPerLevel;
    BoundingBox totalBounds;
};

[[nodiscard]] GeometricLodStats computeGeometricLodStats(const GeometricLodNode& root) noexcept;

// 通用 LOD 统计信息
using LodStats = std::variant<GeoLodStats, GeometricLodStats>;

// 纯函数：计算通用 LOD 统计
[[nodiscard]] LodStats computeLodStats(const LodNode& root) noexcept;

// 辅助函数：检测 LOD 模式
[[nodiscard]] LodMode detectLodMode(const std::variant<geo::GeoBBox, BoundingBox>& bounds) noexcept;

// 辅助函数：从几何包围盒转换为地理包围盒（如果可能）
[[nodiscard]] std::optional<geo::GeoBBox> 
tryConvertToGeoBBox(const BoundingBox& bounds) noexcept;

// 辅助函数：从地理包围盒转换为几何包围盒
[[nodiscard]] BoundingBox convertToBoundingBox(const geo::GeoBBox& geoBounds, double altitude = 0.0) noexcept;

} // namespace lod::core 