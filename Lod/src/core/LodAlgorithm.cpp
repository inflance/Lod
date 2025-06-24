#include "LodAlgorithm.hpp"
#include "core/Geometry.hpp"
#include <meshoptimizer.h>
#include <algorithm>
#include <execution>
#include <cmath>
#include <numeric>

namespace lod::core {

// TriangleCountStrategy 实现
size_t TriangleCountStrategy::targetTriangleCount(const Mesh& mesh, int lodLevel) const {
    size_t currentCount = mesh.triangleCount();
    size_t targetCount = static_cast<size_t>(currentCount * std::pow(reductionRatio_, lodLevel));
    return std::max(targetCount, static_cast<size_t>(100)); // 至少保留100个三角形
}

double TriangleCountStrategy::computeGeometricError(const Mesh& original, const Mesh& simplified) const {
    // 简化的几何误差计算：基于三角形数量比例
    if (original.triangleCount() == 0) {
        return 0.0;
    }
    
    double reductionRatio = 1.0 - static_cast<double>(simplified.triangleCount()) / original.triangleCount();
    return reductionRatio * 100.0; // 将比例转换为误差值
}

bool TriangleCountStrategy::shouldSubdivide(const Mesh& mesh, const geo::GeoBBox& region, int currentLevel) const {
    return mesh.triangleCount() > maxTrianglesPerTile_ && currentLevel < 8;
}

bool TriangleCountStrategy::shouldSubdivide(const Mesh& mesh, const BoundingBox& bounds, int currentLevel) const {
    return mesh.triangleCount() > maxTrianglesPerTile_ && currentLevel < 8;
}

// ScreenSpaceErrorStrategy 实现
size_t ScreenSpaceErrorStrategy::targetTriangleCount(const Mesh& mesh, int lodLevel) const {
    // 基于屏幕空间误差的简化策略
    size_t currentCount = mesh.triangleCount();
    double errorFactor = std::pow(2.0, lodLevel);
    size_t targetCount = static_cast<size_t>(currentCount / errorFactor);
    return std::max(targetCount, static_cast<size_t>(50));
}

double ScreenSpaceErrorStrategy::computeGeometricError(const Mesh& original, const Mesh& simplified) const {
    // 基于屏幕空间误差的计算
    if (original.triangleCount() == 0) {
        return 0.0;
    }
    
    auto originalBounds = computeBoundingBox(original);
    auto simplifiedBounds = computeBoundingBox(simplified);
    
    // 计算包围盒大小差异
    auto originalSize = originalBounds.size();
    auto simplifiedSize = simplifiedBounds.size();
    
    double maxDiff = 0.0;
    for (int i = 0; i < 3; ++i) {
        maxDiff = std::max(maxDiff, std::abs(originalSize[i] - simplifiedSize[i]));
    }
    
    return maxDiff * maxScreenSpaceError_;
}

bool ScreenSpaceErrorStrategy::shouldSubdivide(const Mesh& mesh, const geo::GeoBBox& region, int currentLevel) const {
    // 基于地理区域大小决定是否细分
    double regionSize = std::max(region.width(), region.height());
    return regionSize > 0.01 && currentLevel < 10; // 0.01度约为1km
}

bool ScreenSpaceErrorStrategy::shouldSubdivide(const Mesh& mesh, const BoundingBox& bounds, int currentLevel) const {
    // 基于几何大小决定是否细分
    auto size = bounds.size();
    double maxSize = std::max({size[0], size[1], size[2]});
    return maxSize > 1.0 && currentLevel < 10;
}

// VolumeBasedStrategy 实现
size_t VolumeBasedStrategy::targetTriangleCount(const Mesh& mesh, int lodLevel) const {
    size_t currentCount = mesh.triangleCount();
    size_t targetCount = static_cast<size_t>(currentCount * std::pow(reductionRatio_, lodLevel));
    return std::max(targetCount, static_cast<size_t>(10));
}

double VolumeBasedStrategy::computeGeometricError(const Mesh& original, const Mesh& simplified) const {
    // 基于体积变化的误差计算
    auto originalBounds = computeBoundingBox(original);
    auto simplifiedBounds = computeBoundingBox(simplified);
    
    double originalVolume = originalBounds.volume();
    double simplifiedVolume = simplifiedBounds.volume();
    
    if (originalVolume == 0.0) {
        return 0.0;
    }
    
    return std::abs(originalVolume - simplifiedVolume) / originalVolume * 100.0;
}

bool VolumeBasedStrategy::shouldSubdivide(const Mesh& mesh, const geo::GeoBBox& region, int currentLevel) const {
    // 地理模式下不使用体积策略
    return false;
}

bool VolumeBasedStrategy::shouldSubdivide(const Mesh& mesh, const BoundingBox& bounds, int currentLevel) const {
    return bounds.volume() > minVolumeThreshold_ && currentLevel < 8;
}

// 网格简化函数
Mesh simplifyMesh(const Mesh& mesh, size_t targetTriangleCount) noexcept {
    if (mesh.empty() || mesh.triangleCount() <= targetTriangleCount) {
        return mesh;
    }
    
    const auto& vertices = mesh.vertices();
    const auto& indices = mesh.indices();
    
    // 转换顶点数据为 meshoptimizer 格式（仅使用位置）
    std::vector<float> vertexData;
    vertexData.reserve(vertices.positions.size() * 3);
    
    for (const auto& pos : vertices.positions) {
        vertexData.push_back(pos[0]);
        vertexData.push_back(pos[1]);
        vertexData.push_back(pos[2]);
    }
    
    // 复制索引数据
    std::vector<unsigned int> inputIndices(indices.begin(), indices.end());
    std::vector<unsigned int> outputIndices(indices.size());
    
    // 使用 meshoptimizer 进行简化
    float targetError = 0.01f;
    size_t resultCount = meshopt_simplify(
        outputIndices.data(),
        inputIndices.data(),
        inputIndices.size(),
        vertexData.data(),
        vertices.positions.size(),
        sizeof(float) * 3,
        targetTriangleCount * 3,
        targetError
    );
    
    // 调整输出索引大小
    outputIndices.resize(resultCount);
    
    // 创建简化后的网格
    Mesh::Vertices newVertices = vertices;
    Mesh::Indices newIndices(outputIndices.begin(), outputIndices.end());
    
    return Mesh{std::move(newVertices), std::move(newIndices)};
}

// 地理 LOD 构建
std::shared_ptr<GeoLodNode> buildGeoLodHierarchy(const Mesh& inputMesh, const geo::GeoBBox& region, 
                                                const LodConfig& config) {
    if (inputMesh.empty()) {
        return nullptr;
    }
    
    auto root = std::make_shared<GeoLodNode>();
    root->region = region;
    root->mesh = inputMesh;
    root->lodLevel = 0;
    root->geometricError = 0.0;
    
    // 递归构建函数
    std::function<void(std::shared_ptr<GeoLodNode>, int)> buildRecursive;
    
    buildRecursive = [&](std::shared_ptr<GeoLodNode> node, int depth) {
        if (depth >= config.maxLodLevels) {
            return;
        }
        
        // 检查是否需要细分
        if (!config.strategy->shouldSubdivide(node->mesh, node->region, depth)) {
            return;
        }
        
        // 四叉树细分地理区域
        auto subRegions = node->region.subdivide();
        
        for (const auto& subRegion : subRegions) {
            // 分割网格到子区域
            auto subMeshes = splitMeshByRegion(node->mesh, node->region, {subRegion});
            
            if (!subMeshes.empty() && !subMeshes[0].first.empty()) {
                auto childNode = std::make_shared<GeoLodNode>();
                childNode->region = subRegion;
                childNode->lodLevel = depth + 1;
                
                // 简化子网格
                size_t targetCount = config.strategy->targetTriangleCount(subMeshes[0].first, depth + 1);
                childNode->mesh = simplifyMesh(subMeshes[0].first, targetCount);
                childNode->geometricError = config.strategy->computeGeometricError(subMeshes[0].first, childNode->mesh);
                
                node->children.push_back(childNode);
                
                // 递归构建子节点
                buildRecursive(childNode, depth + 1);
            }
        }
    };
    
    buildRecursive(root, 0);
    return root;
}

// 几何 LOD 构建
std::shared_ptr<GeometricLodNode> buildGeometricLodHierarchy(const Mesh& inputMesh, const BoundingBox& bounds,
                                                            const LodConfig& config) {
    if (config.useOctreeSubdivision) {
        return buildOctreeLodHierarchy(inputMesh, config);
    }
    
    // 传统递归细分方法
    auto root = std::make_shared<GeometricLodNode>();
    root->bounds = bounds;
    root->mesh = inputMesh;
    root->lodLevel = 0;
    root->geometricError = 0.0;
    
    // 递归构建函数
    std::function<void(std::shared_ptr<GeometricLodNode>, int)> buildRecursive;
    
    buildRecursive = [&](std::shared_ptr<GeometricLodNode> node, int depth) {
        if (depth >= config.maxLodLevels) {
            return;
        }
        
        // 检查是否需要细分
        if (!config.strategy->shouldSubdivide(node->mesh, node->bounds, depth)) {
            return;
        }
        
        // 八叉树细分
        auto subBounds = node->bounds.subdivide();
        
        for (const auto& subBound : subBounds) {
            // 分割网格到子区域
            auto subMeshes = splitMeshByBounds(node->mesh, {subBound});
            
            if (!subMeshes.empty() && !subMeshes[0].first.empty()) {
                auto childNode = std::make_shared<GeometricLodNode>();
                childNode->bounds = subBound;
                childNode->lodLevel = depth + 1;
                
                // 简化子网格
                size_t targetCount = config.strategy->targetTriangleCount(subMeshes[0].first, depth + 1);
                childNode->mesh = simplifyMesh(subMeshes[0].first, targetCount);
                childNode->geometricError = config.strategy->computeGeometricError(subMeshes[0].first, childNode->mesh);
                
                node->children.push_back(childNode);
                
                // 递归构建子节点
                buildRecursive(childNode, depth + 1);
            }
        }
    };
    
    buildRecursive(root, 0);
    return root;
}

// 八叉树 LOD 构建
std::shared_ptr<GeometricLodNode> buildOctreeLodHierarchy(const Mesh& inputMesh, const LodConfig& config) {
    return buildGeometricLod(inputMesh, config.octreeConfig);
}

// 通用 LOD 构建
LodNode buildLodHierarchy(const Mesh& inputMesh, 
                         const std::variant<geo::GeoBBox, BoundingBox>& bounds,
                         const LodConfig& config) {
    return std::visit([&](const auto& bound) -> LodNode {
        using T = std::decay_t<decltype(bound)>;
        if constexpr (std::is_same_v<T, geo::GeoBBox>) {
            return buildGeoLodHierarchy(inputMesh, bound, config);
        } else {
            return buildGeometricLodHierarchy(inputMesh, bound, config);
        }
    }, bounds);
}

// 地理区域分割
std::vector<std::pair<Mesh, geo::GeoBBox>> splitMeshByRegion(const Mesh& mesh, const geo::GeoBBox& totalRegion, 
                                                            const std::vector<geo::GeoBBox>& subRegions) {
    // 简化实现：假设网格顶点包含地理坐标信息
    // 实际实现中需要根据具体的坐标转换来分割网格
    std::vector<std::pair<Mesh, geo::GeoBBox>> results;
    
    for (const auto& region : subRegions) {
        // 这里应该实现真正的地理分割逻辑
        // 当前返回整个网格作为占位符
        results.emplace_back(mesh, region);
    }
    
    return results;
}

// 统计计算
GeoLodStats computeGeoLodStats(const GeoLodNode& root) noexcept {
    GeoLodStats stats;
    stats.totalRegion = root.region;
    
    root.traverse([&](const GeoLodNode& node) {
        stats.totalNodes++;
        if (node.isLeaf()) {
            stats.leafNodes++;
        }
        stats.totalTriangles += node.mesh.triangleCount();
        stats.maxDepth = std::max(stats.maxDepth, node.lodLevel);
        
        // 扩展统计数组
        while (static_cast<int>(stats.trianglesPerLevel.size()) <= node.lodLevel) {
            stats.trianglesPerLevel.push_back(0);
        }
        
        stats.trianglesPerLevel[node.lodLevel] += node.mesh.triangleCount();
    });
    
    return stats;
}

GeometricLodStats computeGeometricLodStats(const GeometricLodNode& root) noexcept {
    GeometricLodStats stats;
    stats.totalBounds = root.bounds;
    
    root.traverse([&](const GeometricLodNode& node) {
        stats.totalNodes++;
        if (node.isLeaf()) {
            stats.leafNodes++;
        }
        stats.totalTriangles += node.mesh.triangleCount();
        stats.maxDepth = std::max(stats.maxDepth, node.lodLevel);
        
        // 扩展统计数组
        while (static_cast<int>(stats.trianglesPerLevel.size()) <= node.lodLevel) {
            stats.trianglesPerLevel.push_back(0);
        }
        
        stats.trianglesPerLevel[node.lodLevel] += node.mesh.triangleCount();
    });
    
    return stats;
}

// 模式检测
LodMode detectLodMode(const std::variant<geo::GeoBBox, BoundingBox>& bounds) noexcept {
    return std::visit([](const auto& bound) -> LodMode {
        using T = std::decay_t<decltype(bound)>;
        if constexpr (std::is_same_v<T, geo::GeoBBox>) {
            return LodMode::Geographic;
        } else {
            return LodMode::Geometric;
        }
    }, bounds);
}

// 坐标转换
std::optional<geo::GeoBBox> tryConvertToGeoBBox(const BoundingBox& bounds) noexcept {
    // 简化实现：假设输入已经是地理坐标
    // 实际实现需要进行投影坐标到地理坐标的转换
    return std::nullopt;
}

BoundingBox convertToBoundingBox(const geo::GeoBBox& geoBounds, double altitude) noexcept {
    // 简化实现：直接使用经纬度作为 x,y 坐标
    return BoundingBox{
        {static_cast<float>(geoBounds.minLon), static_cast<float>(geoBounds.minLat), static_cast<float>(altitude)},
        {static_cast<float>(geoBounds.maxLon), static_cast<float>(geoBounds.maxLat), static_cast<float>(altitude)}
    };
}

std::expected<Mesh, std::string> TriangleCountStrategy::simplify(
    const Mesh& mesh, const LodConfig& config) const {
    
    if (mesh.empty()) {
        return std::unexpected("Cannot simplify empty mesh");
    }
    
    if (config.targetTriangleCount >= mesh.triangleCount()) {
        return mesh; // 已经满足目标
    }
    
    // 计算目标索引数
    size_t targetIndexCount = config.targetTriangleCount * 3;
    
    // 使用 meshoptimizer 进行简化
    std::vector<uint32_t> simplified_indices(targetIndexCount);
    
    float target_error = 0.01f;
    size_t result_count = meshopt_simplify(
        simplified_indices.data(),
        mesh.indices.data(),
        mesh.indices.size(),
        reinterpret_cast<const float*>(mesh.vertices.data()),
        mesh.vertices.size(),
        sizeof(Vertex),
        targetIndexCount,
        target_error
    );
    
    simplified_indices.resize(result_count);
    
    // 创建简化后的顶点列表（移除未使用的顶点）
    std::vector<uint32_t> vertex_remap(mesh.vertices.size(), ~0u);
    std::vector<Vertex> simplified_vertices;
    
    for (uint32_t index : simplified_indices) {
        if (vertex_remap[index] == ~0u) {
            vertex_remap[index] = static_cast<uint32_t>(simplified_vertices.size());
            simplified_vertices.push_back(mesh.vertices[index]);
        }
    }
    
    // 重新映射索引
    for (auto& index : simplified_indices) {
        index = vertex_remap[index];
    }
    
    return Mesh{simplified_vertices, simplified_indices};
}

std::expected<Mesh, std::string> ScreenSpaceErrorStrategy::simplify(
    const Mesh& mesh, const LodConfig& config) const {
    
    if (mesh.empty()) {
        return std::unexpected("Cannot simplify empty mesh");
    }
    
    // 计算网格的边界盒以估算屏幕空间误差
    auto bbox = mesh.boundingBox();
    auto diagonal = std::sqrt(
        std::pow(bbox.max.x - bbox.min.x, 2) +
        std::pow(bbox.max.y - bbox.min.y, 2) +
        std::pow(bbox.max.z - bbox.min.z, 2)
    );
    
    // 基于屏幕空间误差计算目标误差
    float target_error = config.maxScreenSpaceError * diagonal / 1000.0f;
    
    // 使用 meshoptimizer 进行基于误差的简化
    std::vector<uint32_t> simplified_indices(mesh.indices.size());
    
    size_t result_count = meshopt_simplify(
        simplified_indices.data(),
        mesh.indices.data(),
        mesh.indices.size(),
        reinterpret_cast<const float*>(mesh.vertices.data()),
        mesh.vertices.size(),
        sizeof(Vertex),
        mesh.indices.size(),
        target_error
    );
    
    simplified_indices.resize(result_count);
    
    // 重新创建顶点列表
    std::vector<uint32_t> vertex_remap(mesh.vertices.size(), ~0u);
    std::vector<Vertex> simplified_vertices;
    
    for (uint32_t index : simplified_indices) {
        if (vertex_remap[index] == ~0u) {
            vertex_remap[index] = static_cast<uint32_t>(simplified_vertices.size());
            simplified_vertices.push_back(mesh.vertices[index]);
        }
    }
    
    for (auto& index : simplified_indices) {
        index = vertex_remap[index];
    }
    
    return Mesh{simplified_vertices, simplified_indices};
}

std::expected<Mesh, std::string> VolumeBasedStrategy::simplify(
    const Mesh& mesh, const LodConfig& config) const {
    
    if (mesh.empty()) {
        return std::unexpected("Cannot simplify empty mesh");
    }
    
    // 基于体积阈值进行简化
    // 这里使用简单的基于三角形面积的过滤
    std::vector<uint32_t> filtered_indices;
    
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        const auto& v0 = mesh.vertices[mesh.indices[i]].position;
        const auto& v1 = mesh.vertices[mesh.indices[i + 1]].position;
        const auto& v2 = mesh.vertices[mesh.indices[i + 2]].position;
        
        // 计算三角形面积
        auto edge1 = Vec3{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
        auto edge2 = Vec3{v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
        
        auto cross = Vec3{
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        };
        
        float area = 0.5f * std::sqrt(cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
        
        // 只保留面积大于阈值的三角形
        if (area >= config.volumeThreshold) {
            filtered_indices.push_back(mesh.indices[i]);
            filtered_indices.push_back(mesh.indices[i + 1]);
            filtered_indices.push_back(mesh.indices[i + 2]);
        }
    }
    
    if (filtered_indices.empty()) {
        return std::unexpected("Volume-based filtering removed all triangles");
    }
    
    // 重新创建顶点列表
    std::vector<uint32_t> vertex_remap(mesh.vertices.size(), ~0u);
    std::vector<Vertex> filtered_vertices;
    
    for (uint32_t index : filtered_indices) {
        if (vertex_remap[index] == ~0u) {
            vertex_remap[index] = static_cast<uint32_t>(filtered_vertices.size());
            filtered_vertices.push_back(mesh.vertices[index]);
        }
    }
    
    for (auto& index : filtered_indices) {
        index = vertex_remap[index];
    }
    
    return Mesh{filtered_vertices, filtered_indices};
}

} // namespace lod::core 