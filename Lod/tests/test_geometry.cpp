#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/core/Geometry.hpp"

using namespace lod::core;

TEST_CASE("BoundingBox basic operations", "[geometry]") {
    SECTION("Default constructor") {
        BoundingBox bbox;
        REQUIRE(bbox.empty());
        REQUIRE(bbox.volume() == Catch::Approx(0.0f));
    }
    
    SECTION("Parameterized constructor") {
        BoundingBox bbox({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f});
        REQUIRE_FALSE(bbox.empty());
        REQUIRE(bbox.volume() == Catch::Approx(8.0f));
        
        auto size = bbox.size();
        REQUIRE(size[0] == Catch::Approx(2.0f));
        REQUIRE(size[1] == Catch::Approx(2.0f));
        REQUIRE(size[2] == Catch::Approx(2.0f));
        
        auto center = bbox.center();
        REQUIRE(center[0] == Catch::Approx(1.0f));
        REQUIRE(center[1] == Catch::Approx(1.0f));
        REQUIRE(center[2] == Catch::Approx(1.0f));
    }
}

TEST_CASE("BoundingBox geometric operations", "[geometry]") {
    BoundingBox bbox1({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f});
    BoundingBox bbox2({1.0f, 1.0f, 1.0f}, {3.0f, 3.0f, 3.0f});
    
    SECTION("Contains point") {
        REQUIRE(bbox1.contains({1.0f, 1.0f, 1.0f}));
        REQUIRE(bbox1.contains({0.0f, 0.0f, 0.0f}));  // 边界点
        REQUIRE(bbox1.contains({2.0f, 2.0f, 2.0f}));  // 边界点
        REQUIRE_FALSE(bbox1.contains({-1.0f, 1.0f, 1.0f}));
        REQUIRE_FALSE(bbox1.contains({3.0f, 1.0f, 1.0f}));
    }
    
    SECTION("Intersection") {
        REQUIRE(bbox1.intersects(bbox2));
        REQUIRE(bbox2.intersects(bbox1));
        
        auto intersection = bbox1.intersection(bbox2);
        REQUIRE(intersection.min[0] == Catch::Approx(1.0f));
        REQUIRE(intersection.min[1] == Catch::Approx(1.0f));
        REQUIRE(intersection.min[2] == Catch::Approx(1.0f));
        REQUIRE(intersection.max[0] == Catch::Approx(2.0f));
        REQUIRE(intersection.max[1] == Catch::Approx(2.0f));
        REQUIRE(intersection.max[2] == Catch::Approx(2.0f));
    }
    
    SECTION("Union") {
        auto union_bbox = bbox1.unite(bbox2);
        REQUIRE(union_bbox.min[0] == Catch::Approx(0.0f));
        REQUIRE(union_bbox.min[1] == Catch::Approx(0.0f));
        REQUIRE(union_bbox.min[2] == Catch::Approx(0.0f));
        REQUIRE(union_bbox.max[0] == Catch::Approx(3.0f));
        REQUIRE(union_bbox.max[1] == Catch::Approx(3.0f));
        REQUIRE(union_bbox.max[2] == Catch::Approx(3.0f));
    }
    
    SECTION("No intersection") {
        BoundingBox bbox3({5.0f, 5.0f, 5.0f}, {7.0f, 7.0f, 7.0f});
        REQUIRE_FALSE(bbox1.intersects(bbox3));
        
        auto intersection = bbox1.intersection(bbox3);
        REQUIRE(intersection.empty());
    }
}

TEST_CASE("BoundingBox octree subdivision", "[geometry]") {
    BoundingBox bbox({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f});
    auto subdivisions = bbox.subdivide();
    
    REQUIRE(subdivisions.size() == 8);
    
    // 验证所有子节点的体积总和等于原包围盒体积
    float totalVolume = 0.0f;
    for (const auto& sub : subdivisions) {
        totalVolume += sub.volume();
        REQUIRE_FALSE(sub.empty());
    }
    REQUIRE(totalVolume == Catch::Approx(bbox.volume()));
    
    // 检查第一个子节点（000: 左下前）
    REQUIRE(subdivisions[0].min[0] == Catch::Approx(0.0f));
    REQUIRE(subdivisions[0].min[1] == Catch::Approx(0.0f));
    REQUIRE(subdivisions[0].min[2] == Catch::Approx(0.0f));
    REQUIRE(subdivisions[0].max[0] == Catch::Approx(1.0f));
    REQUIRE(subdivisions[0].max[1] == Catch::Approx(1.0f));
    REQUIRE(subdivisions[0].max[2] == Catch::Approx(1.0f));
    
    // 检查最后一个子节点（111: 右上后）
    REQUIRE(subdivisions[7].min[0] == Catch::Approx(1.0f));
    REQUIRE(subdivisions[7].min[1] == Catch::Approx(1.0f));
    REQUIRE(subdivisions[7].min[2] == Catch::Approx(1.0f));
    REQUIRE(subdivisions[7].max[0] == Catch::Approx(2.0f));
    REQUIRE(subdivisions[7].max[1] == Catch::Approx(2.0f));
    REQUIRE(subdivisions[7].max[2] == Catch::Approx(2.0f));
}

TEST_CASE("OctreeNode basic operations", "[octree]") {
    SECTION("Empty octree node") {
        OctreeNode node;
        node.bounds = BoundingBox({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
        
        REQUIRE(node.isLeaf());
        REQUIRE(node.triangleCount() == 0);
        REQUIRE(node.depth == 0);
    }
    
    SECTION("Octree node with triangles") {
        OctreeNode node;
        node.bounds = BoundingBox({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
        node.triangleIndices = {0, 1, 2, 3, 4, 5};
        node.depth = 1;
        
        REQUIRE(node.isLeaf());
        REQUIRE(node.triangleCount() == 6);
        REQUIRE(node.depth == 1);
    }
    
    SECTION("Octree node with children") {
        OctreeNode root;
        root.bounds = BoundingBox({0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f});
        
        // 添加一个子节点
        root.children[0] = std::make_unique<OctreeNode>();
        root.children[0]->bounds = BoundingBox({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
        root.children[0]->depth = 1;
        
        REQUIRE_FALSE(root.isLeaf());
        REQUIRE(root.children[0]->isLeaf());
    }
}

TEST_CASE("OctreeConfig validation", "[octree]") {
    SECTION("Default config") {
        OctreeConfig config;
        REQUIRE(config.maxTrianglesPerNode == 1000);
        REQUIRE(config.maxDepth == 8);
        REQUIRE(config.minNodeSize == Catch::Approx(0.001f));
        REQUIRE(config.enableAdaptiveSubdivision == true);
    }
    
    SECTION("Custom config") {
        OctreeConfig config;
        config.maxTrianglesPerNode = 500;
        config.maxDepth = 6;
        config.minNodeSize = 0.01f;
        config.enableAdaptiveSubdivision = false;
        
        REQUIRE(config.maxTrianglesPerNode == 500);
        REQUIRE(config.maxDepth == 6);
        REQUIRE(config.minNodeSize == Catch::Approx(0.01f));
        REQUIRE(config.enableAdaptiveSubdivision == false);
    }
}

TEST_CASE("GeometricLodNode operations", "[geometric_lod]") {
    SECTION("Empty geometric LOD node") {
        GeometricLodNode node;
        REQUIRE(node.isLeaf());
        REQUIRE(node.childCount() == 0);
        REQUIRE(node.lodLevel == 0);
        REQUIRE(node.geometricError == Catch::Approx(0.0));
    }
    
    SECTION("Geometric LOD node with children") {
        auto root = std::make_shared<GeometricLodNode>();
        root->bounds = BoundingBox({0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f});
        root->lodLevel = 0;
        
        auto child1 = std::make_shared<GeometricLodNode>();
        child1->bounds = BoundingBox({0.0f, 0.0f, 0.0f}, {5.0f, 5.0f, 5.0f});
        child1->lodLevel = 1;
        
        auto child2 = std::make_shared<GeometricLodNode>();
        child2->bounds = BoundingBox({5.0f, 5.0f, 5.0f}, {10.0f, 10.0f, 10.0f});
        child2->lodLevel = 1;
        
        root->children = {child1, child2};
        
        REQUIRE_FALSE(root->isLeaf());
        REQUIRE(root->childCount() == 2);
        REQUIRE(child1->isLeaf());
        REQUIRE(child2->isLeaf());
    }
    
    SECTION("Geometric LOD traversal") {
        auto root = std::make_shared<GeometricLodNode>();
        root->lodLevel = 0;
        
        auto child = std::make_shared<GeometricLodNode>();
        child->lodLevel = 1;
        root->children = {child};
        
        std::vector<int> visitedLevels;
        root->traverse([&visitedLevels](const GeometricLodNode& node) {
            visitedLevels.push_back(node.lodLevel);
        });
        
        REQUIRE(visitedLevels.size() == 2);
        REQUIRE(visitedLevels[0] == 0);
        REQUIRE(visitedLevels[1] == 1);
    }
} 