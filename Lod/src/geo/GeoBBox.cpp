#define _USE_MATH_DEFINES
#include "geo/GeoBBox.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace lod::geo {

namespace {
    // 地球半径（米）
    constexpr double EARTH_RADIUS = 6378137.0;
    
    // 角度转弧度
    constexpr double toRadians(double degrees) {
        return degrees * M_PI / 180.0;
    }
    
    // 弧度转角度
    constexpr double toDegrees(double radians) {
        return radians * 180.0 / M_PI;
    }
}

std::optional<GeoBBox> computeBounds(std::span<const GeoPoint> points) noexcept {
    if (points.empty()) {
        return std::nullopt;
    }
    
    auto minLon = points[0].longitude;
    auto maxLon = points[0].longitude;
    auto minLat = points[0].latitude;
    auto maxLat = points[0].latitude;
    
    for (const auto& point : points) {
        minLon = std::min(minLon, point.longitude);
        maxLon = std::max(maxLon, point.longitude);
        minLat = std::min(minLat, point.latitude);
        maxLat = std::max(maxLat, point.latitude);
    }
    
    return GeoBBox{minLon, minLat, maxLon, maxLat};
}

double distanceMeters(const GeoPoint& p1, const GeoPoint& p2) noexcept {
    // 使用 Haversine 公式计算球面距离
    const double lat1 = toRadians(p1.latitude);
    const double lat2 = toRadians(p2.latitude);
    const double deltaLat = toRadians(p2.latitude - p1.latitude);
    const double deltaLon = toRadians(p2.longitude - p1.longitude);
    
    const double a = std::sin(deltaLat / 2) * std::sin(deltaLat / 2) +
                     std::cos(lat1) * std::cos(lat2) *
                     std::sin(deltaLon / 2) * std::sin(deltaLon / 2);
    
    const double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    
    return EARTH_RADIUS * c;
}

double areaSquareMeters(const GeoBBox& bbox) noexcept {
    if (bbox.empty()) {
        return 0.0;
    }
    
    // 近似计算：对于小区域，使用简化的投影面积
    const double lat1 = toRadians(bbox.minLat);
    const double lat2 = toRadians(bbox.maxLat);
    const double deltaLon = toRadians(bbox.width());
    const double deltaLat = toRadians(bbox.height());
    
    // 使用平均纬度来修正经度的变形
    const double avgLat = (lat1 + lat2) / 2;
    const double adjustedDeltaLon = deltaLon * std::cos(avgLat);
    
    // 转换为米并计算面积
    const double latMeters = deltaLat * EARTH_RADIUS;
    const double lonMeters = adjustedDeltaLon * EARTH_RADIUS;
    
    return latMeters * lonMeters;
}

} // namespace lod::geo 