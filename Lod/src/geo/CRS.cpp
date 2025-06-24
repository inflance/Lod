#include "CRS.hpp"
#include <algorithm>
#include <unordered_map>

namespace lod::geo {

// CRS 实现
bool CRS::isGeographic() const noexcept {
    // 简化实现：检查是否为 EPSG:4326 或其他地理坐标系
    return code_ == "EPSG:4326" || code_ == "EPSG:4269" || code_ == "EPSG:4979";
}

bool CRS::isProjected() const noexcept {
    // 简化实现：检查是否为投影坐标系
    return !isGeographic() && (code_.find("EPSG:") == 0);
}

std::string CRS::getUnit() const noexcept {
    if (isGeographic()) {
        return "degree";
    } else if (isProjected()) {
        return "metre";
    } else {
        return "unknown";
    }
}

// CoordinateTransformer 实现
CoordinateTransformer::CoordinateTransformer(const CRS& sourceCRS, const CRS& targetCRS)
    : sourceCRS_(sourceCRS), targetCRS_(targetCRS) {
}

std::optional<GeoPoint> CoordinateTransformer::transform(const GeoPoint& point) const {
    // 简化实现：当前仅支持相同 CRS 之间的"转换"
    if (sourceCRS_.code() == targetCRS_.code()) {
        return point;
    }
    
    // 这里应该使用 PROJ 库进行实际的坐标转换
    // 当前返回原点作为占位符
    return std::nullopt;
}

std::optional<GeoBBox> CoordinateTransformer::transform(const GeoBBox& bbox) const {
    // 简化实现：转换包围盒的四个角点
    if (sourceCRS_.code() == targetCRS_.code()) {
        return bbox;
    }
    
    // 当前返回原包围盒作为占位符
    return std::nullopt;
}

std::vector<GeoPoint> CoordinateTransformer::transform(const std::vector<GeoPoint>& points) const {
    std::vector<GeoPoint> result;
    result.reserve(points.size());
    
    for (const auto& point : points) {
        auto transformedPoint = transform(point);
        if (transformedPoint) {
            result.push_back(*transformedPoint);
        }
    }
    
    return result;
}

std::optional<std::array<double, 3>> CoordinateTransformer::transformInternal(const std::array<double, 3>& coords) const {
    // 这里应该实现具体的坐标转换逻辑
    // 当前简化实现直接返回输入坐标
    return coords;
}

// 工厂函数实现
std::optional<CRS> createCRS(const std::string& code) {
    if (isValidCRS(code)) {
        return CRS{code};
    }
    return std::nullopt;
}

std::optional<CRS> parseCRSFromString(const std::string& crsString) {
    // 简化解析：支持 "EPSG:xxxx" 格式
    if (crsString.find("EPSG:") == 0 && crsString.length() > 5) {
        try {
            int epsgCode = std::stoi(crsString.substr(5));
            if (epsgCode > 0) {
                return CRS{crsString};
            }
        } catch (const std::exception&) {
            // 解析失败
        }
    }
    
    return std::nullopt;
}

bool isValidCRS(const std::string& code) noexcept {
    // 简化验证：检查常见的 EPSG 代码
    static const std::unordered_map<std::string, bool> validCodes = {
        {"EPSG:4326", true},  // WGS84
        {"EPSG:3857", true},  // Web Mercator
        {"EPSG:4269", true},  // NAD83
        {"EPSG:4979", true},  // WGS84 3D
        {"EPSG:32649", true}, // UTM Zone 49N
        {"EPSG:32650", true}, // UTM Zone 50N
        {"EPSG:2154", true},  // RGF93 / Lambert-93
        {"EPSG:25832", true}, // ETRS89 / UTM zone 32N
    };
    
    auto it = validCodes.find(code);
    return it != validCodes.end() && it->second;
}

std::vector<std::string> getSupportedCRS() noexcept {
    return {
        "EPSG:4326",   // WGS84 Geographic
        "EPSG:3857",   // Web Mercator
        "EPSG:4269",   // NAD83 Geographic
        "EPSG:4979",   // WGS84 3D Geographic
        "EPSG:32649",  // UTM Zone 49N
        "EPSG:32650",  // UTM Zone 50N
        "EPSG:2154",   // RGF93 / Lambert-93
        "EPSG:25832",  // ETRS89 / UTM zone 32N
    };
}

} // namespace lod::geo 