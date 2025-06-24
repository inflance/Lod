#include "core/Mesh.hpp"
#include <algorithm>
#include <numeric>
#include <execution>
#include <set>
#include <unordered_map>
#include <iterator>
#include <cmath>

namespace lod::core {

Mesh Mesh::subset(std::span<const Index> triangleIndices) const {
    if (triangleIndices.empty() || indices_.empty()) {
        return Mesh{};
    }
    
    // 收集所有被引用的顶点索引
    std::set<Index> usedVertices;
    Indices newIndices;
    newIndices.reserve(triangleIndices.size() * 3);
    
    for (const auto triIndex : triangleIndices) {
        if (triIndex * 3 + 2 < indices_.size()) {
            const auto i0 = indices_[triIndex * 3];
            const auto i1 = indices_[triIndex * 3 + 1];
            const auto i2 = indices_[triIndex * 3 + 2];
            
            usedVertices.insert(i0);
            usedVertices.insert(i1);
            usedVertices.insert(i2);
        }
    }
    
    // 创建顶点重映射表
    std::unordered_map<Index, Index> vertexRemap;
    Vertices newVertices;
    newVertices.reserve(usedVertices.size());
    
    Index newIndex = 0;
    for (const auto oldIndex : usedVertices) {
        if (oldIndex < vertices_.size()) {
            vertexRemap[oldIndex] = newIndex++;
            
            // 复制顶点属性
            if (oldIndex < vertices_.positions.size()) {
                newVertices.positions.push_back(vertices_.positions[oldIndex]);
            }
            if (oldIndex < vertices_.normals.size()) {
                newVertices.normals.push_back(vertices_.normals[oldIndex]);
            }
            if (oldIndex < vertices_.texCoords.size()) {
                newVertices.texCoords.push_back(vertices_.texCoords[oldIndex]);
            }
            if (oldIndex < vertices_.colors.size()) {
                newVertices.colors.push_back(vertices_.colors[oldIndex]);
            }
        }
    }
    
    // 重新构建索引
    for (const auto triIndex : triangleIndices) {
        if (triIndex * 3 + 2 < indices_.size()) {
            const auto i0 = indices_[triIndex * 3];
            const auto i1 = indices_[triIndex * 3 + 1];
            const auto i2 = indices_[triIndex * 3 + 2];
            
            auto it0 = vertexRemap.find(i0);
            auto it1 = vertexRemap.find(i1);
            auto it2 = vertexRemap.find(i2);
            
            if (it0 != vertexRemap.end() && it1 != vertexRemap.end() && it2 != vertexRemap.end()) {
                newIndices.push_back(it0->second);
                newIndices.push_back(it1->second);
                newIndices.push_back(it2->second);
            }
        }
    }
    
    return Mesh{std::move(newVertices), std::move(newIndices)};
}

Mesh Mesh::merge(std::span<const Mesh> meshes) {
    if (meshes.empty()) {
        return Mesh{};
    }
    
    if (meshes.size() == 1) {
        return meshes[0];
    }
    
    // 计算总顶点和索引数量
    size_t totalVertices = 0;
    size_t totalIndices = 0;
    
    for (const auto& mesh : meshes) {
        totalVertices += mesh.vertexCount();
        totalIndices += mesh.indices().size();
    }
    
    // 预分配空间
    Vertices mergedVertices;
    mergedVertices.reserve(totalVertices);
    
    Indices mergedIndices;
    mergedIndices.reserve(totalIndices);
    
    Index vertexOffset = 0;
    
    // 合并所有网格
    for (const auto& mesh : meshes) {
        const auto& vertices = mesh.vertices();
        const auto& indices = mesh.indices();
        
        // 复制顶点属性
        mergedVertices.positions.insert(mergedVertices.positions.end(),
                                       vertices.positions.begin(), vertices.positions.end());
        mergedVertices.normals.insert(mergedVertices.normals.end(),
                                     vertices.normals.begin(), vertices.normals.end());
        mergedVertices.texCoords.insert(mergedVertices.texCoords.end(),
                                       vertices.texCoords.begin(), vertices.texCoords.end());
        mergedVertices.colors.insert(mergedVertices.colors.end(),
                                    vertices.colors.begin(), vertices.colors.end());
        
        // 复制索引（需要加上偏移）
        std::transform(indices.begin(), indices.end(), std::back_inserter(mergedIndices),
                      [vertexOffset](Index idx) { return idx + vertexOffset; });
        
        vertexOffset += static_cast<Index>(vertices.size());
    }
    
    return Mesh{std::move(mergedVertices), std::move(mergedIndices)};
}

MeshStats computeStats(const Mesh& mesh) noexcept {
    MeshStats stats;
    
    if (mesh.empty()) {
        return stats;
    }
    
    stats.vertexCount = mesh.vertexCount();
    stats.triangleCount = mesh.triangleCount();
    
    const auto& positions = mesh.vertices().positions;
    
    if (!positions.empty()) {
        // 初始化包围盒
        stats.boundingBoxMin = positions[0];
        stats.boundingBoxMax = positions[0];
        
        // 计算包围盒
        for (const auto& pos : positions) {
            for (int i = 0; i < 3; ++i) {
                stats.boundingBoxMin[i] = std::min(stats.boundingBoxMin[i], pos[i]);
                stats.boundingBoxMax[i] = std::max(stats.boundingBoxMax[i], pos[i]);
            }
        }
        
        // 计算表面积（简化版本）
        const auto& indices = mesh.indices();
        for (size_t i = 0; i < indices.size(); i += 3) {
            if (i + 2 < indices.size()) {
                const auto& v0 = positions[indices[i]];
                const auto& v1 = positions[indices[i + 1]];
                const auto& v2 = positions[indices[i + 2]];
                
                // 计算三角形面积（向量叉积）
                std::array<float, 3> edge1 = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
                std::array<float, 3> edge2 = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};
                
                std::array<float, 3> cross = {
                    edge1[1] * edge2[2] - edge1[2] * edge2[1],
                    edge1[2] * edge2[0] - edge1[0] * edge2[2],
                    edge1[0] * edge2[1] - edge1[1] * edge2[0]
                };
                
                float length = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
                stats.surfaceArea += length * 0.5f;
            }
        }
    }
    
    return stats;
}

std::pair<Vertex, Vertex> computeBoundingBox(const Mesh& mesh) noexcept {
    if (mesh.vertices.empty()) {
        return {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    }
    
    auto min_vertex = mesh.vertices[0];
    auto max_vertex = mesh.vertices[0];
    
    for (const auto& vertex : mesh.vertices) {
        min_vertex.position.x = std::min(min_vertex.position.x, vertex.position.x);
        min_vertex.position.y = std::min(min_vertex.position.y, vertex.position.y);
        min_vertex.position.z = std::min(min_vertex.position.z, vertex.position.z);
        
        max_vertex.position.x = std::max(max_vertex.position.x, vertex.position.x);
        max_vertex.position.y = std::max(max_vertex.position.y, vertex.position.y);
        max_vertex.position.z = std::max(max_vertex.position.z, vertex.position.z);
    }
    
    return {min_vertex, max_vertex};
}

Mesh& Mesh::operator=(const Mesh& other) {
    if (this != &other) {
        vertices = other.vertices;
        indices = other.indices;
        meshId = other.meshId;
    }
    return *this;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        vertices = std::move(other.vertices);
        indices = std::move(other.indices);
        meshId = std::move(other.meshId);
    }
    return *this;
}

bool Mesh::empty() const noexcept {
    return vertices.empty() || indices.empty();
}

size_t Mesh::triangleCount() const noexcept {
    return indices.size() / 3;
}

size_t Mesh::vertexCount() const noexcept {
    return vertices.size();
}

BoundingBox Mesh::boundingBox() const noexcept {
    if (vertices.empty()) {
        return BoundingBox{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    }
    
    auto min_pos = vertices[0].position;
    auto max_pos = vertices[0].position;
    
    for (const auto& vertex : vertices) {
        min_pos.x = std::min(min_pos.x, vertex.position.x);
        min_pos.y = std::min(min_pos.y, vertex.position.y);
        min_pos.z = std::min(min_pos.z, vertex.position.z);
        
        max_pos.x = std::max(max_pos.x, vertex.position.x);
        max_pos.y = std::max(max_pos.y, vertex.position.y);
        max_pos.z = std::max(max_pos.z, vertex.position.z);
    }
    
    return BoundingBox{min_pos, max_pos};
}

std::expected<Mesh, std::string> Mesh::merge(const std::vector<Mesh>& meshes) {
    if (meshes.empty()) {
        return std::unexpected("Cannot merge empty mesh list");
    }
    
    if (meshes.size() == 1) {
        return meshes[0];
    }
    
    // 计算总顶点和索引数量
    size_t total_vertices = 0;
    size_t total_indices = 0;
    
    for (const auto& mesh : meshes) {
        total_vertices += mesh.vertices.size();
        total_indices += mesh.indices.size();
    }
    
    // 创建合并后的网格
    std::vector<Vertex> merged_vertices;
    std::vector<uint32_t> merged_indices;
    
    merged_vertices.reserve(total_vertices);
    merged_indices.reserve(total_indices);
    
    uint32_t vertex_offset = 0;
    
    for (const auto& mesh : meshes) {
        // 复制顶点
        std::copy(mesh.vertices.begin(), mesh.vertices.end(), 
                  std::back_inserter(merged_vertices));
        
        // 复制索引并调整偏移
        for (uint32_t index : mesh.indices) {
            merged_indices.push_back(index + vertex_offset);
        }
        
        vertex_offset += static_cast<uint32_t>(mesh.vertices.size());
    }
    
    return Mesh{merged_vertices, merged_indices};
}

std::expected<Mesh, std::string> Mesh::subset(const std::vector<uint32_t>& triangle_indices) const {
    if (triangle_indices.empty()) {
        return std::unexpected("Cannot create subset from empty triangle list");
    }
    
    // 验证三角形索引有效性
    for (uint32_t tri_idx : triangle_indices) {
        if (tri_idx >= triangleCount()) {
            return std::unexpected("Triangle index out of range");
        }
    }
    
    // 收集所有需要的顶点索引
    std::set<uint32_t> vertex_indices_set;
    for (uint32_t tri_idx : triangle_indices) {
        vertex_indices_set.insert(indices[tri_idx * 3]);
        vertex_indices_set.insert(indices[tri_idx * 3 + 1]);
        vertex_indices_set.insert(indices[tri_idx * 3 + 2]);
    }
    
    // 创建顶点重映射
    std::vector<uint32_t> vertex_indices(vertex_indices_set.begin(), vertex_indices_set.end());
    std::map<uint32_t, uint32_t> vertex_remap;
    
    for (size_t i = 0; i < vertex_indices.size(); ++i) {
        vertex_remap[vertex_indices[i]] = static_cast<uint32_t>(i);
    }
    
    // 创建子集顶点
    std::vector<Vertex> subset_vertices;
    subset_vertices.reserve(vertex_indices.size());
    
    for (uint32_t old_idx : vertex_indices) {
        subset_vertices.push_back(vertices[old_idx]);
    }
    
    // 创建子集索引
    std::vector<uint32_t> subset_indices;
    subset_indices.reserve(triangle_indices.size() * 3);
    
    for (uint32_t tri_idx : triangle_indices) {
        subset_indices.push_back(vertex_remap[indices[tri_idx * 3]]);
        subset_indices.push_back(vertex_remap[indices[tri_idx * 3 + 1]]);
        subset_indices.push_back(vertex_remap[indices[tri_idx * 3 + 2]]);
    }
    
    return Mesh{subset_vertices, subset_indices};
}

MeshStatistics Mesh::statistics() const noexcept {
    MeshStatistics stats;
    
    stats.vertexCount = vertices.size();
    stats.triangleCount = triangleCount();
    stats.boundingBox = boundingBox();
    
    // 计算表面积
    for (size_t i = 0; i < indices.size(); i += 3) {
        const auto& v0 = vertices[indices[i]].position;
        const auto& v1 = vertices[indices[i + 1]].position;
        const auto& v2 = vertices[indices[i + 2]].position;
        
        // 使用叉积计算三角形面积
        auto edge1 = Vec3{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
        auto edge2 = Vec3{v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
        
        auto cross = Vec3{
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        };
        
        float area = 0.5f * std::sqrt(cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
        stats.surfaceArea += area;
    }
    
    return stats;
}

} // namespace lod::core 