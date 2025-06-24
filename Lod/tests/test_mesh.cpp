#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/core/Mesh.hpp"

using namespace lod::core;

TEST_CASE("Mesh basic operations", "[mesh]") {
    SECTION("Empty mesh") {
        Mesh mesh;
        REQUIRE(mesh.empty());
        REQUIRE(mesh.vertexCount() == 0);
        REQUIRE(mesh.triangleCount() == 0);
    }
    
    SECTION("Simple triangle mesh") {
        VertexAttributes vertices;
        vertices.positions = {
            {0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {0.5f, 1.0f, 0.0f}
        };
        
        std::vector<Index> indices = {0, 1, 2};
        
        Mesh mesh(std::move(vertices), std::move(indices));
        
        REQUIRE_FALSE(mesh.empty());
        REQUIRE(mesh.vertexCount() == 3);
        REQUIRE(mesh.triangleCount() == 1);
    }
    
    SECTION("Mesh with attributes") {
        VertexAttributes vertices;
        vertices.positions = {
            {0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {0.5f, 1.0f, 0.0f}
        };
        vertices.normals = {
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f}
        };
        vertices.colors = {
            {255, 0, 0, 255},
            {0, 255, 0, 255},
            {0, 0, 255, 255}
        };
        
        std::vector<Index> indices = {0, 1, 2};
        
        Mesh mesh(std::move(vertices), std::move(indices));
        
        REQUIRE(mesh.vertices().normals.size() == 3);
        REQUIRE(mesh.vertices().colors.size() == 3);
    }
}

TEST_CASE("Mesh statistics", "[mesh]") {
    VertexAttributes vertices;
    vertices.positions = {
        {0.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f},
        {1.0f, 2.0f, 0.0f},
        {1.0f, 1.0f, 1.0f}
    };
    
    std::vector<Index> indices = {0, 1, 2, 0, 2, 3, 1, 3, 2, 0, 3, 1};
    
    Mesh mesh(std::move(vertices), std::move(indices));
    
    auto stats = computeStats(mesh);
    
    REQUIRE(stats.vertexCount == 4);
    REQUIRE(stats.triangleCount == 4);
    
    // 检查包围盒
    REQUIRE(stats.boundingBoxMin[0] == Catch::Approx(0.0f));
    REQUIRE(stats.boundingBoxMin[1] == Catch::Approx(0.0f));
    REQUIRE(stats.boundingBoxMin[2] == Catch::Approx(0.0f));
    
    REQUIRE(stats.boundingBoxMax[0] == Catch::Approx(2.0f));
    REQUIRE(stats.boundingBoxMax[1] == Catch::Approx(2.0f));
    REQUIRE(stats.boundingBoxMax[2] == Catch::Approx(1.0f));
}

TEST_CASE("Mesh functional operations", "[mesh]") {
    VertexAttributes vertices;
    vertices.positions = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.5f, 1.0f, 0.0f}
    };
    
    std::vector<Index> indices = {0, 1, 2};
    
    Mesh originalMesh(vertices, indices);
    
    SECTION("withVertices creates new mesh") {
        VertexAttributes newVertices;
        newVertices.positions = {
            {0.0f, 0.0f, 0.0f},
            {2.0f, 0.0f, 0.0f},
            {1.0f, 2.0f, 0.0f}
        };
        
        auto newMesh = originalMesh.withVertices(std::move(newVertices));
        
        // 原网格不变
        REQUIRE(originalMesh.vertices().positions[1][0] == Catch::Approx(1.0f));
        
        // 新网格有新的顶点
        REQUIRE(newMesh.vertices().positions[1][0] == Catch::Approx(2.0f));
        REQUIRE(newMesh.vertices().positions[2][1] == Catch::Approx(2.0f));
        
        // 索引保持不变
        REQUIRE(newMesh.indices() == originalMesh.indices());
    }
    
    SECTION("withIndices creates new mesh") {
        std::vector<Index> newIndices = {2, 1, 0};  // 反向
        
        auto newMesh = originalMesh.withIndices(std::move(newIndices));
        
        // 顶点保持不变
        REQUIRE(newMesh.vertices().positions == originalMesh.vertices().positions);
        
        // 索引改变
        REQUIRE(newMesh.indices()[0] == 2);
        REQUIRE(newMesh.indices()[1] == 1);
        REQUIRE(newMesh.indices()[2] == 0);
    }
} 