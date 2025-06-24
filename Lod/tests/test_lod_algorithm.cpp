#include "catch2/catch_test_macros.hpp"
#include "core/LodAlgorithm.hpp"
#include "core/Mesh.hpp"

TEST_CASE("LOD Algorithm - TriangleCountStrategy", "[lod_algorithm]") {
    using namespace lod;
    
    auto strategy = std::make_unique<TriangleCountStrategy>();
    
    SECTION("Basic functionality") {
        // 创建一个简单的网格用于测试
        std::vector<lod::Vertex> vertices = {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
        };
        std::vector<uint32_t> indices = {0, 1, 2};
        
        lod::Mesh mesh{vertices, indices};
        
        // 简化到1个三角形（这个网格本来就只有1个三角形）
        lod::LodConfig config;
        config.targetTriangleCount = 1;
        
        auto result = strategy->simplify(mesh, config);
        REQUIRE(result.has_value());
        REQUIRE(result->indices.size() == 3); // 1个三角形 = 3个索引
    }
}

TEST_CASE("LOD Algorithm - ScreenSpaceErrorStrategy", "[lod_algorithm]") {
    using namespace lod;
    
    auto strategy = std::make_unique<ScreenSpaceErrorStrategy>();
    
    SECTION("Basic functionality") {
        std::vector<lod::Vertex> vertices = {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
        };
        std::vector<uint32_t> indices = {0, 1, 2};
        
        lod::Mesh mesh{vertices, indices};
        
        lod::LodConfig config;
        config.maxScreenSpaceError = 1.0f;
        
        auto result = strategy->simplify(mesh, config);
        REQUIRE(result.has_value());
        // 屏幕空间误差策略可能保留原始网格
        REQUIRE(result->indices.size() >= 3);
    }
}

TEST_CASE("LOD Algorithm - VolumeBasedStrategy", "[lod_algorithm]") {
    using namespace lod;
    
    auto strategy = std::make_unique<VolumeBasedStrategy>();
    
    SECTION("Basic functionality") {
        std::vector<lod::Vertex> vertices = {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
        };
        std::vector<uint32_t> indices = {0, 1, 2};
        
        lod::Mesh mesh{vertices, indices};
        
        lod::LodConfig config;
        config.volumeThreshold = 0.1f;
        
        auto result = strategy->simplify(mesh, config);
        REQUIRE(result.has_value());
        REQUIRE(result->indices.size() >= 3);
    }
}

TEST_CASE("LOD Algorithm - GeometricLodBuilder", "[lod_algorithm]") {
    using namespace lod;
    
    GeometricLodBuilder builder;
    
    SECTION("Build LOD hierarchy") {
        std::vector<lod::Vertex> vertices = {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
        };
        std::vector<uint32_t> indices = {0, 1, 2, 1, 3, 2};
        
        lod::Mesh mesh{vertices, indices};
        
        lod::LodConfig config;
        config.maxLevels = 3;
        config.reductionFactor = 0.5f;
        
        auto result = builder.buildLodHierarchy(mesh, config);
        REQUIRE(result.has_value());
        REQUIRE(result->levels.size() <= config.maxLevels);
    }
} 