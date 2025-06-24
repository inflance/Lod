#pragma once

#include <vector>
#include <array>
#include <memory>
#include <span>

namespace lod::core {

// 基础数据类型
using Vertex = std::array<float, 3>;
using Normal = std::array<float, 3>;
using TexCoord = std::array<float, 2>;
using Color = std::array<uint8_t, 4>;
using Index = uint32_t;

// 顶点属性集合
struct VertexAttributes {
    std::vector<Vertex> positions;
    std::vector<Normal> normals;
    std::vector<TexCoord> texCoords;
    std::vector<Color> colors;
    
    constexpr size_t size() const noexcept { return positions.size(); }
    constexpr bool empty() const noexcept { return positions.empty(); }
    
    void reserve(size_t count) {
        positions.reserve(count);
        normals.reserve(count);
        texCoords.reserve(count);
        colors.reserve(count);
    }
    
    void clear() noexcept {
        positions.clear();
        normals.clear();
        texCoords.clear();
        colors.clear();
    }
};

// 不可变网格结构
class Mesh {
public:
    using Vertices = VertexAttributes;
    using Indices = std::vector<Index>;
    
    Mesh() = default;
    Mesh(Vertices vertices, Indices indices) 
        : vertices_(std::move(vertices)), indices_(std::move(indices)) {}
    
    // 访问器（只读）
    const Vertices& vertices() const noexcept { return vertices_; }
    const Indices& indices() const noexcept { return indices_; }
    
    // 查询方法
    constexpr size_t vertexCount() const noexcept { return vertices_.size(); }
    constexpr size_t triangleCount() const noexcept { return indices_.size() / 3; }
    constexpr bool empty() const noexcept { return vertices_.empty() || indices_.empty(); }
    
    // 创建新实例的函数式操作
    [[nodiscard]] Mesh withVertices(Vertices newVertices) const {
        return Mesh{std::move(newVertices), indices_};
    }
    
    [[nodiscard]] Mesh withIndices(Indices newIndices) const {
        return Mesh{vertices_, std::move(newIndices)};
    }
    
    // 子网格提取
    [[nodiscard]] Mesh subset(std::span<const Index> triangleIndices) const;
    
    // 网格合并
    [[nodiscard]] static Mesh merge(std::span<const Mesh> meshes);
    
private:
    Vertices vertices_;
    Indices indices_;
};

// 网格统计信息
struct MeshStats {
    size_t vertexCount{0};
    size_t triangleCount{0};
    std::array<float, 3> boundingBoxMin{0, 0, 0};
    std::array<float, 3> boundingBoxMax{0, 0, 0};
    float surfaceArea{0.0f};
};

// 纯函数：计算网格统计
[[nodiscard]] MeshStats computeStats(const Mesh& mesh) noexcept;

// 纯函数：计算包围盒
[[nodiscard]] std::pair<Vertex, Vertex> computeBoundingBox(const Mesh& mesh) noexcept;

} // namespace lod::core 