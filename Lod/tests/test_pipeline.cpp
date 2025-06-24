#include "catch2/catch_test_macros.hpp"
#include "pipeline/LodPipeline.hpp"

TEST_CASE("LOD Pipeline - Basic Configuration", "[pipeline]") {
    using namespace lod;
    
    SECTION("Create default pipeline") {
        auto pipeline = LodPipeline::builder()
            .withGeometricMode()
            .withTriangleCountStrategy()
            .build();
        
        REQUIRE(pipeline.has_value());
    }
    
    SECTION("Create geographic pipeline") {
        auto pipeline = LodPipeline::builder()
            .withGeographicMode("EPSG:4326")
            .withScreenSpaceErrorStrategy()
            .build();
        
        REQUIRE(pipeline.has_value());
    }
}

TEST_CASE("LOD Pipeline - Configuration validation", "[pipeline]") {
    using namespace lod;
    
    SECTION("Invalid CRS should fail") {
        auto pipeline = LodPipeline::builder()
            .withGeographicMode("INVALID_CRS")
            .build();
        
        REQUIRE_FALSE(pipeline.has_value());
    }
}

TEST_CASE("LOD Pipeline - Processing steps", "[pipeline]") {
    using namespace lod;
    
    SECTION("Empty input should be handled") {
        auto pipeline = LodPipeline::builder()
            .withGeometricMode()
            .withTriangleCountStrategy()
            .build();
        
        REQUIRE(pipeline.has_value());
        
        // 测试空输入的处理
        std::vector<std::string> emptyInput;
        LodConfig config;
        
        // 这应该返回错误或空结果，而不是崩溃
        // 具体行为取决于实现
    }
} 