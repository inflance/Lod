#include "core/Geometry.hpp"
#include <algorithm>
#include <stack>
#include <execution>
#include <functional>
#include <numeric>
#include <cmath>

namespace lod::core {

BoundingBox computeGeometryBoundingBox(const Mesh& mesh) noexcept {
    if (mesh.vertices.empty()) {
        return BoundingBox{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    }
    
    auto min_pos = mesh.vertices[0].position;
    auto max_pos = mesh.vertices[0].position;
    
    for (const auto& vertex : mesh.vertices) {
        min_pos.x = std::min(min_pos.x, vertex.position.x);
        min_pos.y = std::min(min_pos.y, vertex.position.y);
        min_pos.z = std::min(min_pos.z, vertex.position.z);
        
        max_pos.x = std::max(max_pos.x, vertex.position.x);
        max_pos.y = std::max(max_pos.y, vertex.position.y);
        max_pos.z = std::max(max_pos.z, vertex.position.z);
    }
    
    return BoundingBox{min_pos, max_pos};
}

bool triangleIntersectsBounds(const std::array<Vertex, 3>& triangle, 
                             const BoundingBox& bounds) noexcept {
    // 首先检查三角形的包围盒是否与给定包围盒相交
    auto triBounds = computeTriangleBounds(triangle);
    if (!bounds.intersects(triBounds)) {
        return false;
    }
    
    // 如果三角形包围盒相交，进行更精确的检测
    // 简化版本：检查三角形顶点是否在包围盒内或包围盒顶点是否在三角形内
    for (const auto& vertex : triangle) {
        if (bounds.contains(vertex)) {
            return true;
        }
    }
    
    // 更复杂的相交检测可以在这里实现
    return true;  // 保守估计
}

BoundingBox computeTriangleBounds(const std::array<Vertex, 3>& triangle) noexcept {
    auto minPos = triangle[0];
    auto maxPos = triangle[0];
    
    for (const auto& vertex : triangle) {
        for (int i = 0; i < 3; ++i) {
            minPos[i] = std::min(minPos[i], vertex[i]);
            maxPos[i] = std::max(maxPos[i], vertex[i]);
        }
    }
    
    return BoundingBox{minPos, maxPos};
}

std::unique_ptr<OctreeNode> buildOctree(const Mesh& mesh, const OctreeConfig& config) {
    if (mesh.empty()) {
        return nullptr;
    }
    
    auto rootBounds = computeBoundingBox(mesh);
    if (rootBounds.empty()) {
        return nullptr;
    }
    
    auto root = std::make_unique<OctreeNode>();
    root->bounds = rootBounds;
    root->depth = 0;
    
    // 初始化根节点包含所有三角形
    const size_t triangleCount = mesh.triangleCount();
    root->triangleIndices.reserve(triangleCount);
    for (size_t i = 0; i < triangleCount; ++i) {
        root->triangleIndices.push_back(static_cast<Index>(i));
    }
    
    // 递归构建八叉树
    std::function<void(OctreeNode&)> subdivideNode = [&](OctreeNode& node) {
        // 检查是否需要细分
        if (node.triangleIndices.size() <= config.maxTrianglesPerNode ||
            node.depth >= config.maxDepth ||
            node.bounds.volume() < config.minNodeSize * config.minNodeSize * config.minNodeSize) {
            return;
        }
        
        // 八叉树细分
        auto childBounds = node.bounds.subdivide();
        const auto& positions = mesh.vertices().positions;
        const auto& indices = mesh.indices();
        
        // 为每个子节点分配三角形
        for (int childIdx = 0; childIdx < 8; ++childIdx) {
            std::vector<Index> childTriangles;
            
            for (const auto triIdx : node.triangleIndices) {
                if (triIdx * 3 + 2 < indices.size()) {
                    // 获取三角形的三个顶点
                    std::array<Vertex, 3> triangle = {
                        positions[indices[triIdx * 3]],
                        positions[indices[triIdx * 3 + 1]],
                        positions[indices[triIdx * 3 + 2]]
                    };
                    
                    // 检查三角形是否与子包围盒相交
                    if (triangleIntersectsBounds(triangle, childBounds[childIdx])) {
                        childTriangles.push_back(triIdx);
                    }
                }
            }
            
            // 如果子节点包含三角形，创建子节点
            if (!childTriangles.empty()) {
                auto childNode = std::make_unique<OctreeNode>();
                childNode->bounds = childBounds[childIdx];
                childNode->triangleIndices = std::move(childTriangles);
                childNode->depth = node.depth + 1;
                
                node.children[childIdx] = std::move(childNode);
                
                // 递归细分子节点
                subdivideNode(*node.children[childIdx]);
            }
        }
        
        // 如果成功创建了子节点，清空父节点的三角形列表（避免重复）
        bool hasChildren = std::any_of(node.children.begin(), node.children.end(),
                                      [](const auto& child) { return child != nullptr; });
        if (hasChildren) {
            node.triangleIndices.clear();
        }
    };
    
    subdivideNode(*root);
    return root;
}

std::shared_ptr<GeometricLodNode> buildGeometricLod(const Mesh& mesh, const OctreeConfig& octreeConfig) {
    if (mesh.empty()) {
        return nullptr;
    }
    
    auto octree = buildOctree(mesh, octreeConfig);
    if (!octree) {
        return nullptr;
    }
    
    auto root = std::make_shared<GeometricLodNode>();
    root->bounds = octree->bounds;
    root->mesh = mesh;
    root->lodLevel = 0;
    root->geometricError = 0.0;
    
    // 从八叉树构建 LOD 层次结构的递归函数
    std::function<std::shared_ptr<GeometricLodNode>(const OctreeNode&, int)> buildLodFromOctree;
    
    buildLodFromOctree = [&](const OctreeNode& octreeNode, int lodLevel) -> std::shared_ptr<GeometricLodNode> {
        auto lodNode = std::make_shared<GeometricLodNode>();
        lodNode->bounds = octreeNode.bounds;
        lodNode->lodLevel = lodLevel;
        
        // 如果是叶节点，创建网格
        if (octreeNode.isLeaf()) {
            if (!octreeNode.triangleIndices.empty()) {
                lodNode->mesh = mesh.subset(octreeNode.triangleIndices);
            }
        } else {
            // 为非叶节点创建简化网格
            std::vector<Index> allTriangles;
            octreeNode.traverse([&](const OctreeNode& node) {
                if (node.isLeaf()) {
                    allTriangles.insert(allTriangles.end(), 
                                       node.triangleIndices.begin(), 
                                       node.triangleIndices.end());
                }
            });
            
            if (!allTriangles.empty()) {
                lodNode->mesh = mesh.subset(allTriangles);
            }
            
            // 递归创建子节点
            for (const auto& child : octreeNode.children) {
                if (child) {
                    auto childLodNode = buildLodFromOctree(*child, lodLevel + 1);
                    if (childLodNode && !childLodNode->mesh.empty()) {
                        lodNode->children.push_back(childLodNode);
                    }
                }
            }
        }
        
        return lodNode;
    };
    
    return buildLodFromOctree(*octree, 0);
}

std::vector<std::pair<Mesh, BoundingBox>> splitMeshByBounds(const Mesh& mesh, const std::vector<BoundingBox>& bounds) {
    std::vector<std::pair<Mesh, BoundingBox>> results;
    results.reserve(bounds.size());
    
    const auto& positions = mesh.vertices().positions;
    const auto& indices = mesh.indices();
    
    for (const auto& bound : bounds) {
        std::vector<Index> triangleIndices;
        
        // 找到与此包围盒相交的所有三角形
        for (size_t i = 0; i < mesh.triangleCount(); ++i) {
            if (i * 3 + 2 < indices.size()) {
                std::array<Vertex, 3> triangle = {
                    positions[indices[i * 3]],
                    positions[indices[i * 3 + 1]],
                    positions[indices[i * 3 + 2]]
                };
                
                if (triangleIntersectsBounds(triangle, bound)) {
                    triangleIndices.push_back(static_cast<Index>(i));
                }
            }
        }
        
        if (!triangleIndices.empty()) {
            auto subMesh = mesh.subset(triangleIndices);
            results.emplace_back(std::move(subMesh), bound);
        }
    }
    
    return results;
}

OctreeStats computeOctreeStats(const OctreeNode& root) noexcept {
    OctreeStats stats;
    
    root.traverse([&](const OctreeNode& node) {
        stats.totalNodes++;
        if (node.isLeaf()) {
            stats.leafNodes++;
        }
        stats.totalTriangles += node.triangleIndices.size();
        stats.maxDepth = std::max(stats.maxDepth, node.depth);
        
        // 扩展统计数组
        while (static_cast<int>(stats.trianglesPerLevel.size()) <= node.depth) {
            stats.trianglesPerLevel.push_back(0);
            stats.nodesPerLevel.push_back(0);
        }
        
        stats.trianglesPerLevel[node.depth] += node.triangleIndices.size();
        stats.nodesPerLevel[node.depth]++;
    });
    
    return stats;
}

std::expected<std::vector<Octree>, std::string> 
buildOctree(const Mesh& mesh, int maxDepth, size_t maxTrianglesPerNode) {
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return std::unexpected("Cannot build octree for empty mesh");
    }
    
    std::vector<Octree> octrees;
    
    // 计算根节点的边界盒
    auto boundingBox = computeGeometryBoundingBox(mesh);
    
    // 计算三角形中心点
    std::vector<Vec3> triangleCenters;
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        const auto& v0 = mesh.vertices[mesh.indices[i]].position;
        const auto& v1 = mesh.vertices[mesh.indices[i + 1]].position;
        const auto& v2 = mesh.vertices[mesh.indices[i + 2]].position;
        
        Vec3 center = {
            (v0.x + v1.x + v2.x) / 3.0f,
            (v0.y + v1.y + v2.y) / 3.0f,
            (v0.z + v1.z + v2.z) / 3.0f
        };
        triangleCenters.push_back(center);
    }
    
    // 递归构建八叉树
    auto buildNode = [&](auto& self, const BoundingBox& bounds, 
                         const std::vector<uint32_t>& triangleIndices, 
                         int depth) -> std::expected<Octree, std::string> {
        
        Octree node;
        node.bounds = bounds;
        node.triangleIndices = triangleIndices;
        node.depth = depth;
        
        // 如果达到最大深度或三角形数量小于阈值，创建叶节点
        if (depth >= maxDepth || triangleIndices.size() <= maxTrianglesPerNode) {
            // 这是一个叶节点
            return node;
        }
        
        // 计算八个子节点的边界
        Vec3 center = {
            (bounds.min.x + bounds.max.x) / 2.0f,
            (bounds.min.y + bounds.max.y) / 2.0f,
            (bounds.min.z + bounds.max.z) / 2.0f
        };
        
        std::array<BoundingBox, 8> childBounds = {{
            {{bounds.min.x, bounds.min.y, bounds.min.z}, {center.x, center.y, center.z}},
            {{center.x, bounds.min.y, bounds.min.z}, {bounds.max.x, center.y, center.z}},
            {{bounds.min.x, center.y, bounds.min.z}, {center.x, bounds.max.y, center.z}},
            {{center.x, center.y, bounds.min.z}, {bounds.max.x, bounds.max.y, center.z}},
            {{bounds.min.x, bounds.min.y, center.z}, {center.x, center.y, bounds.max.z}},
            {{center.x, bounds.min.y, center.z}, {bounds.max.x, center.y, bounds.max.z}},
            {{bounds.min.x, center.y, center.z}, {center.x, bounds.max.y, bounds.max.z}},
            {{center.x, center.y, center.z}, {bounds.max.x, bounds.max.y, bounds.max.z}}
        }};
        
        // 将三角形分配到子节点
        std::array<std::vector<uint32_t>, 8> childTriangles;
        
        for (uint32_t triIdx : triangleIndices) {
            const Vec3& triCenter = triangleCenters[triIdx];
            
            // 确定三角形属于哪个子节点
            int childIndex = 0;
            if (triCenter.x >= center.x) childIndex |= 1;
            if (triCenter.y >= center.y) childIndex |= 2;
            if (triCenter.z >= center.z) childIndex |= 4;
            
            childTriangles[childIndex].push_back(triIdx);
        }
        
        // 递归创建子节点
        for (int i = 0; i < 8; ++i) {
            if (!childTriangles[i].empty()) {
                auto childResult = self(self, childBounds[i], childTriangles[i], depth + 1);
                if (!childResult) {
                    return childResult;
                }
                node.children.push_back(std::move(*childResult));
            }
        }
        
        return node;
    };
    
    // 创建所有三角形的索引列表
    std::vector<uint32_t> allTriangles;
    for (size_t i = 0; i < mesh.indices.size() / 3; ++i) {
        allTriangles.push_back(static_cast<uint32_t>(i));
    }
    
    auto rootResult = buildNode(buildNode, boundingBox, allTriangles, 0);
    if (!rootResult) {
        return std::unexpected(rootResult.error());
    }
    
    octrees.push_back(std::move(*rootResult));
    return octrees;
}

std::expected<std::vector<Mesh>, std::string> 
extractGeometricLevels(const std::vector<Octree>& octrees, const Mesh& originalMesh) {
    if (octrees.empty()) {
        return std::unexpected("Cannot extract levels from empty octree");
    }
    
    std::vector<Mesh> levels;
    
    // 从每个八叉树提取不同级别的网格
    for (const auto& octree : octrees) {
        // 收集所有叶节点的三角形
        std::vector<uint32_t> leafTriangles;
        
        std::function<void(const Octree&)> collectLeafTriangles = [&](const Octree& node) {
            if (node.children.empty()) {
                // 这是叶节点
                leafTriangles.insert(leafTriangles.end(), 
                                   node.triangleIndices.begin(), 
                                   node.triangleIndices.end());
            } else {
                // 递归处理子节点
                for (const auto& child : node.children) {
                    collectLeafTriangles(child);
                }
            }
        };
        
        collectLeafTriangles(octree);
        
        // 基于收集到的三角形创建网格
        auto levelMesh = originalMesh.subset(leafTriangles);
        if (!levelMesh) {
            return std::unexpected(levelMesh.error());
        }
        
        levels.push_back(std::move(*levelMesh));
    }
    
    return levels;
}

bool triangleIntersectsBoundingBox(const Vec3& v0, const Vec3& v1, const Vec3& v2, 
                                  const BoundingBox& bbox) noexcept {
    // 首先检查三角形的所有顶点是否都在包围盒外的同一侧
    auto isOutside = [&](const Vec3& point) {
        return point.x < bbox.min.x || point.x > bbox.max.x ||
               point.y < bbox.min.y || point.y > bbox.max.y ||
               point.z < bbox.min.z || point.z > bbox.max.z;
    };
    
    // 如果任何顶点在包围盒内，则相交
    if (!isOutside(v0) || !isOutside(v1) || !isOutside(v2)) {
        return true;
    }
    
    // 简化的SAT（分离轴定理）检测
    // 这里使用简单的包围盒vs三角形包围盒检测
    Vec3 triMin = {
        std::min({v0.x, v1.x, v2.x}),
        std::min({v0.y, v1.y, v2.y}),
        std::min({v0.z, v1.z, v2.z})
    };
    
    Vec3 triMax = {
        std::max({v0.x, v1.x, v2.x}),
        std::max({v0.y, v1.y, v2.y}),
        std::max({v0.z, v1.z, v2.z})
    };
    
    // 检查三角形包围盒与节点包围盒是否相交
    return !(triMin.x > bbox.max.x || triMax.x < bbox.min.x ||
             triMin.y > bbox.max.y || triMax.y < bbox.min.y ||
             triMin.z > bbox.max.z || triMax.z < bbox.min.z);
}

} // namespace lod::core 