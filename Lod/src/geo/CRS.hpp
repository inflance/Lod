#pragma once

#include "GeoBBox.hpp"
#include <string>
#include <optional>
#include <array>

namespace lod::geo {

// 坐标参考系统类
class CRS {
public:
    explicit CRS(const std::string& code) : code_(code) {}
    
    // 获取 CRS 代码
    const std::string& code() const noexcept { return code_; }
    
    // 判断是否为地理坐标系
    bool isGeographic() const noexcept;
    
    // 判断是否为投影坐标系
    bool isProjected() const noexcept;
    
    // 获取单位（米、度等）
    std::string getUnit() const noexcept;
    
private:
    std::string code_;
};

// 坐标转换器
class CoordinateTransformer {
public:
    CoordinateTransformer(const CRS& sourceCRS, const CRS& targetCRS);
    
    // 转换单个点
    std::optional<GeoPoint> transform(const GeoPoint& point) const;
    
    // 转换包围盒
    std::optional<GeoBBox> transform(const GeoBBox& bbox) const;
    
    // 批量转换
    std::vector<GeoPoint> transform(const std::vector<GeoPoint>& points) const;
    
private:
    CRS sourceCRS_;
    CRS targetCRS_;
    
    // 内部转换实现
    std::optional<std::array<double, 3>> transformInternal(const std::array<double, 3>& coords) const;
};

// 常用的坐标参考系统
namespace crs {
    const CRS WGS84("EPSG:4326");          // WGS84 地理坐标系
    const CRS WebMercator("EPSG:3857");    // Web 墨卡托投影
    const CRS UTM_Zone_49N("EPSG:32649");  // UTM 49N 投影
    const CRS UTM_Zone_50N("EPSG:32650");  // UTM 50N 投影
}

// 工厂函数
[[nodiscard]] std::optional<CRS> createCRS(const std::string& code);

// 辅助函数：从字符串解析 CRS
[[nodiscard]] std::optional<CRS> parseCRSFromString(const std::string& crsString);

// 辅助函数：检查 CRS 是否有效
[[nodiscard]] bool isValidCRS(const std::string& code) noexcept;

// 辅助函数：获取支持的 CRS 列表
[[nodiscard]] std::vector<std::string> getSupportedCRS() noexcept;

} // namespace lod::geo 