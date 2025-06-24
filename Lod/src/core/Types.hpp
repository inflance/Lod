#pragma once
#include <vector>
#include <cstdint>
#include <array>
#include <string>
#include <optional>

namespace lod::core {

// 基本类型
struct Vec3 {
    float x, y, z;
};

struct Vertex {
    Vec3 position;
    Vec3 normal;
    std::array<float, 2> texCoords;
};

struct BoundingBox {
    Vec3 min;
    Vec3 max;
};

// 简化的网格结构
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    bool empty() const { return vertices.empty() || indices.empty(); }
    size_t triangleCount() const { return indices.size() / 3; }
    size_t vertexCount() const { return vertices.size(); }
};

// LOD 配置
struct LodConfig {
    int maxLevels = 4;
    size_t targetTriangleCount = 1000;
    float maxScreenSpaceError = 1.0f;
    float volumeThreshold = 0.01f;
    float reductionFactor = 0.5f;
};

} // namespace lod::core 