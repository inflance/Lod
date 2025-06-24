#pragma once

#include "Mesh.hpp"
#include <array>
#include <vector>
#include <memory>
#include <optional>

namespace lod::core {

// 3D 包围盒
struct BoundingBox {
    std::array<float, 3> min{0.0f, 0.0f, 0.0f};
    std::array<float, 3> max{0.0f, 0.0f, 0.0f};
    
    constexpr BoundingBox() = default;
    constexpr BoundingBox(const std::array<float, 3>& min, const std::array<float, 3>& max)
        : min(min), max(max) {}
    
    // 查询方法
    constexpr std::array<float, 3> size() const noexcept {
        return {max[0] - min[0], max[1] - min[1], max[2] - min[2]};
    }
    
    constexpr std::array<float, 3> center() const noexcept {
        return {(min[0] + max[0]) * 0.5f, (min[1] + max[1]) * 0.5f, (min[2] + max[2]) * 0.5f};
    }
    
    constexpr float volume() const noexcept {
        auto s = size();
        return s[0] * s[1] * s[2];
    }
    
    constexpr bool empty() const noexcept {
        return min[0] >= max[0] || min[1] >= max[1] || min[2] >= max[2];
    }
    
    // 几何操作
    constexpr bool contains(const std::array<float, 3>& point) const noexcept {
        return point[0] >= min[0] && point[0] <= max[0] &&
               point[1] >= min[1] && point[1] <= max[1] &&
               point[2] >= min[2] && point[2] <= max[2];
    }
    
    constexpr bool intersects(const BoundingBox& other) const noexcept {
        return !(max[0] < other.min[0] || min[0] > other.max[0] ||
                 max[1] < other.min[1] || min[1] > other.max[1] ||
                 max[2] < other.min[2] || min[2] > other.max[2]);
    }
    
    constexpr BoundingBox intersection(const BoundingBox& other) const noexcept {
        return BoundingBox{
            {std::max(min[0], other.min[0]), std::max(min[1], other.min[1]), std::max(min[2], other.min[2])},
            {std::min(max[0], other.max[0]), std::min(max[1], other.max[1]), std::min(max[2], other.max[2])}
        };
    }
    
    constexpr BoundingBox unite(const BoundingBox& other) const noexcept {
        return BoundingBox{
            {std::min(min[0], other.min[0]), std::min(min[1], other.min[1]), std::min(min[2], other.min[2])},
            {std::max(max[0], other.max[0]), std::max(max[1], other.max[1]), std::max(max[2], other.max[2])}
        };
    }
    
    // 八叉树分割
    constexpr std::array<BoundingBox, 8> subdivide() const noexcept {
        auto c = center();
        
        return {{
            // 下层 4 个子节点
            {min, c},                                                    // 000: 左下前
            {{c[0], min[1], min[2]}, {max[0], c[1], c[2]}},             // 100: 右下前
            {{min[0], c[1], min[2]}, {c[0], max[1], c[2]}},             // 010: 左上前
            {{c[0], c[1], min[2]}, {max[0], max[1], c[2]}},             // 110: 右上前
            
            // 上层 4 个子节点
            {{min[0], min[1], c[2]}, {c[0], c[1], max[2]}},             // 001: 左下后
            {{c[0], min[1], c[2]}, {max[0], c[1], max[2]}},             // 101: 右下后
            {{min[0], c[1], c[2]}, {c[0], max[1], max[2]}},             // 011: 左上后
            {c, max}                                                     // 111: 右上后
        }};
    }
};

// 八叉树节点
struct OctreeNode {
    BoundingBox bounds;
    std::vector<Index> triangleIndices;  // 该节点包含的三角形索引
    std::array<std::unique_ptr<OctreeNode>, 8> children;
    int depth{0};
    
    // 查询方法
    bool isLeaf() const noexcept {
        return std::all_of(children.begin(), children.end(), 
                          [](const auto& child) { return child == nullptr; });
    }
    
    size_t triangleCount() const noexcept { return triangleIndices.size(); }
    
    // 遍历器
    template<typename Visitor>
    void traverse(Visitor&& visitor) const {
        visitor(*this);
        for (const auto& child : children) {
            if (child) {
                child->traverse(std::forward<Visitor>(visitor));
            }
        }
    }
    
    // 叶节点收集
    void collectLeaves(std::vector<std::reference_wrapper<const OctreeNode>>& leaves) const {
        if (isLeaf()) {
            leaves.emplace_back(*this);
        } else {
            for (const auto& child : children) {
                if (child) {
                    child->collectLeaves(leaves);
                }
            }
        }
    }
};

// 八叉树构建配置
struct OctreeConfig {
    size_t maxTrianglesPerNode{1000};     // 每个节点最大三角形数
    int maxDepth{8};                      // 最大深度
    float minNodeSize{0.001f};            // 最小节点尺寸
    bool enableAdaptiveSubdivision{true}; // 自适应细分
};

// 几何 LOD 节点（用于非地理坐标情况）
struct GeometricLodNode {
    BoundingBox bounds;
    std::vector<std::shared_ptr<GeometricLodNode>> children;
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

// 纯函数：从网格计算包围盒
[[nodiscard]] BoundingBox computeBoundingBox(const Mesh& mesh) noexcept;

// 纯函数：构建八叉树
[[nodiscard]] std::unique_ptr<OctreeNode> 
buildOctree(const Mesh& mesh, const OctreeConfig& config = {});

// 纯函数：从八叉树构建几何 LOD
[[nodiscard]] std::shared_ptr<GeometricLodNode>
buildGeometricLod(const Mesh& mesh, const OctreeConfig& octreeConfig = {});

// 纯函数：判断三角形是否与包围盒相交
[[nodiscard]] bool triangleIntersectsBounds(
    const std::array<Vertex, 3>& triangle, 
    const BoundingBox& bounds) noexcept;

// 纯函数：计算三角形的包围盒
[[nodiscard]] BoundingBox computeTriangleBounds(const std::array<Vertex, 3>& triangle) noexcept;

// 纯函数：根据包围盒分割网格
[[nodiscard]] std::vector<std::pair<Mesh, BoundingBox>>
splitMeshByBounds(const Mesh& mesh, const std::vector<BoundingBox>& bounds);

// 纯函数：计算八叉树统计信息
struct OctreeStats {
    size_t totalNodes{0};
    size_t leafNodes{0};
    size_t totalTriangles{0};
    int maxDepth{0};
    std::vector<size_t> trianglesPerLevel;
    std::vector<size_t> nodesPerLevel;
};

[[nodiscard]] OctreeStats computeOctreeStats(const OctreeNode& root) noexcept;

} // namespace lod::core 