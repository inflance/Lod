#pragma once

#include <array>
#include <vector>
#include <optional>

namespace lod::geo {

// WGS84 地理坐标包围盒
struct GeoBBox {
    double minLon{0.0};
    double minLat{0.0};
    double maxLon{0.0};
    double maxLat{0.0};
    
    constexpr GeoBBox() = default;
    constexpr GeoBBox(double minLon, double minLat, double maxLon, double maxLat)
        : minLon(minLon), minLat(minLat), maxLon(maxLon), maxLat(maxLat) {}
    
    // 查询方法
    constexpr double width() const noexcept { return maxLon - minLon; }
    constexpr double height() const noexcept { return maxLat - minLat; }
    constexpr double centerLon() const noexcept { return (minLon + maxLon) * 0.5; }
    constexpr double centerLat() const noexcept { return (minLat + maxLat) * 0.5; }
    constexpr bool empty() const noexcept { return width() <= 0.0 || height() <= 0.0; }
    
    // 几何操作
    constexpr bool contains(double lon, double lat) const noexcept {
        return lon >= minLon && lon <= maxLon && lat >= minLat && lat <= maxLat;
    }
    
    constexpr bool intersects(const GeoBBox& other) const noexcept {
        return !(maxLon < other.minLon || minLon > other.maxLon ||
                 maxLat < other.minLat || minLat > other.maxLat);
    }
    
    constexpr GeoBBox intersection(const GeoBBox& other) const noexcept {
        return GeoBBox{
            std::max(minLon, other.minLon),
            std::max(minLat, other.minLat),
            std::min(maxLon, other.maxLon),
            std::min(maxLat, other.maxLat)
        };
    }
    
    constexpr GeoBBox unite(const GeoBBox& other) const noexcept {
        return GeoBBox{
            std::min(minLon, other.minLon),
            std::min(minLat, other.minLat),
            std::max(maxLon, other.maxLon),
            std::max(maxLat, other.maxLat)
        };
    }
    
    // 四叉树分割
    constexpr std::array<GeoBBox, 4> subdivide() const noexcept {
        const double midLon = centerLon();
        const double midLat = centerLat();
        
        return {{
            {minLon, minLat, midLon, midLat},  // SW
            {midLon, minLat, maxLon, midLat},  // SE
            {minLon, midLat, midLon, maxLat},  // NW
            {midLon, midLat, maxLon, maxLat}   // NE
        }};
    }
};

// 地理坐标点
struct GeoPoint {
    double longitude{0.0};
    double latitude{0.0};
    double altitude{0.0};  // 海拔高度（米）
    
    constexpr GeoPoint() = default;
    constexpr GeoPoint(double lon, double lat, double alt = 0.0)
        : longitude(lon), latitude(lat), altitude(alt) {}
};

// 纯函数：从点集合计算包围盒
[[nodiscard]] std::optional<GeoBBox> computeBounds(const std::vector<GeoPoint>& points) noexcept;

// 纯函数：计算两点间距离（米）
[[nodiscard]] double distanceMeters(const GeoPoint& p1, const GeoPoint& p2) noexcept;

// 纯函数：包围盒面积（平方米）
[[nodiscard]] double areaSquareMeters(const GeoBBox& bbox) noexcept;

// 计算多个地理点的边界框
GeoBBox computeBounds(const std::vector<GeoPoint>& points);

} // namespace lod::geo 