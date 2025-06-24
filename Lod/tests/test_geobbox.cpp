#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/geo/GeoBBox.hpp"

using namespace lod::geo;

TEST_CASE("GeoBBox basic operations", "[geobbox]") {
    SECTION("Default constructor") {
        GeoBBox bbox;
        REQUIRE(bbox.minLon == 0.0);
        REQUIRE(bbox.minLat == 0.0);
        REQUIRE(bbox.maxLon == 0.0);
        REQUIRE(bbox.maxLat == 0.0);
        REQUIRE(bbox.empty());
    }
    
    SECTION("Parameterized constructor") {
        GeoBBox bbox(-180.0, -90.0, 180.0, 90.0);
        REQUIRE(bbox.minLon == -180.0);
        REQUIRE(bbox.minLat == -90.0);
        REQUIRE(bbox.maxLon == 180.0);
        REQUIRE(bbox.maxLat == 90.0);
        REQUIRE_FALSE(bbox.empty());
    }
    
    SECTION("Dimensions") {
        GeoBBox bbox(100.0, 30.0, 120.0, 50.0);
        REQUIRE(bbox.width() == Catch::Approx(20.0));
        REQUIRE(bbox.height() == Catch::Approx(20.0));
        REQUIRE(bbox.centerLon() == Catch::Approx(110.0));
        REQUIRE(bbox.centerLat() == Catch::Approx(40.0));
    }
}

TEST_CASE("GeoBBox geometric operations", "[geobbox]") {
    GeoBBox bbox1(100.0, 30.0, 120.0, 50.0);
    GeoBBox bbox2(110.0, 40.0, 130.0, 60.0);
    
    SECTION("Contains point") {
        REQUIRE(bbox1.contains(110.0, 40.0));
        REQUIRE(bbox1.contains(100.0, 30.0));  // 边界点
        REQUIRE(bbox1.contains(120.0, 50.0));  // 边界点
        REQUIRE_FALSE(bbox1.contains(90.0, 40.0));
        REQUIRE_FALSE(bbox1.contains(110.0, 60.0));
    }
    
    SECTION("Intersection") {
        REQUIRE(bbox1.intersects(bbox2));
        REQUIRE(bbox2.intersects(bbox1));
        
        auto intersection = bbox1.intersection(bbox2);
        REQUIRE(intersection.minLon == Catch::Approx(110.0));
        REQUIRE(intersection.minLat == Catch::Approx(40.0));
        REQUIRE(intersection.maxLon == Catch::Approx(120.0));
        REQUIRE(intersection.maxLat == Catch::Approx(50.0));
    }
    
    SECTION("Union") {
        auto union_bbox = bbox1.unite(bbox2);
        REQUIRE(union_bbox.minLon == Catch::Approx(100.0));
        REQUIRE(union_bbox.minLat == Catch::Approx(30.0));
        REQUIRE(union_bbox.maxLon == Catch::Approx(130.0));
        REQUIRE(union_bbox.maxLat == Catch::Approx(60.0));
    }
    
    SECTION("No intersection") {
        GeoBBox bbox3(150.0, 70.0, 170.0, 80.0);
        REQUIRE_FALSE(bbox1.intersects(bbox3));
        
        auto intersection = bbox1.intersection(bbox3);
        REQUIRE(intersection.empty());
    }
}

TEST_CASE("GeoBBox subdivision", "[geobbox]") {
    GeoBBox bbox(100.0, 30.0, 120.0, 50.0);
    auto subdivisions = bbox.subdivide();
    
    REQUIRE(subdivisions.size() == 4);
    
    // SW (South-West)
    REQUIRE(subdivisions[0].minLon == Catch::Approx(100.0));
    REQUIRE(subdivisions[0].minLat == Catch::Approx(30.0));
    REQUIRE(subdivisions[0].maxLon == Catch::Approx(110.0));
    REQUIRE(subdivisions[0].maxLat == Catch::Approx(40.0));
    
    // SE (South-East)
    REQUIRE(subdivisions[1].minLon == Catch::Approx(110.0));
    REQUIRE(subdivisions[1].minLat == Catch::Approx(30.0));
    REQUIRE(subdivisions[1].maxLon == Catch::Approx(120.0));
    REQUIRE(subdivisions[1].maxLat == Catch::Approx(40.0));
    
    // NW (North-West)
    REQUIRE(subdivisions[2].minLon == Catch::Approx(100.0));
    REQUIRE(subdivisions[2].minLat == Catch::Approx(40.0));
    REQUIRE(subdivisions[2].maxLon == Catch::Approx(110.0));
    REQUIRE(subdivisions[2].maxLat == Catch::Approx(50.0));
    
    // NE (North-East)
    REQUIRE(subdivisions[3].minLon == Catch::Approx(110.0));
    REQUIRE(subdivisions[3].minLat == Catch::Approx(40.0));
    REQUIRE(subdivisions[3].maxLon == Catch::Approx(120.0));
    REQUIRE(subdivisions[3].maxLat == Catch::Approx(50.0));
    
    // 验证子区域完全覆盖原区域
    auto union_all = subdivisions[0].unite(subdivisions[1]).unite(subdivisions[2]).unite(subdivisions[3]);
    REQUIRE(union_all.minLon == Catch::Approx(bbox.minLon));
    REQUIRE(union_all.minLat == Catch::Approx(bbox.minLat));
    REQUIRE(union_all.maxLon == Catch::Approx(bbox.maxLon));
    REQUIRE(union_all.maxLat == Catch::Approx(bbox.maxLat));
}

TEST_CASE("GeoPoint operations", "[geopoint]") {
    SECTION("Default constructor") {
        GeoPoint point;
        REQUIRE(point.longitude == 0.0);
        REQUIRE(point.latitude == 0.0);
        REQUIRE(point.altitude == 0.0);
    }
    
    SECTION("Parameterized constructor") {
        GeoPoint point(120.0, 30.0, 100.0);
        REQUIRE(point.longitude == 120.0);
        REQUIRE(point.latitude == 30.0);
        REQUIRE(point.altitude == 100.0);
    }
}

TEST_CASE("GeoBBox utility functions", "[geobbox]") {
    SECTION("Compute bounds from points") {
        std::vector<GeoPoint> points = {
            {100.0, 30.0, 0.0},
            {120.0, 50.0, 100.0},
            {110.0, 40.0, 50.0},
            {105.0, 35.0, 200.0}
        };
        
        auto bounds = computeBounds(points);
        REQUIRE(bounds.has_value());
        
        REQUIRE(bounds->minLon == Catch::Approx(100.0));
        REQUIRE(bounds->minLat == Catch::Approx(30.0));
        REQUIRE(bounds->maxLon == Catch::Approx(120.0));
        REQUIRE(bounds->maxLat == Catch::Approx(50.0));
    }
    
    SECTION("Empty points") {
        std::vector<GeoPoint> points;
        auto bounds = computeBounds(points);
        REQUIRE_FALSE(bounds.has_value());
    }
    
    SECTION("Distance calculation") {
        GeoPoint p1(0.0, 0.0);
        GeoPoint p2(1.0, 0.0);
        
        auto distance = distanceMeters(p1, p2);
        REQUIRE(distance > 110000.0);  // 大约111公里
        REQUIRE(distance < 112000.0);
    }
    
    SECTION("Area calculation") {
        GeoBBox bbox(0.0, 0.0, 1.0, 1.0);  // 1度x1度
        auto area = areaSquareMeters(bbox);
        REQUIRE(area > 0.0);
        REQUIRE(area < 20000000000.0);  // 合理的面积范围
    }
} 