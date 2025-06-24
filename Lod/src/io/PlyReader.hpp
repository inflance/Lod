#pragma once

#include "../core/Mesh.hpp"
#include "../core/Geometry.hpp"
#include "../geo/GeoBBox.hpp"
#include <string>
#include <vector>
#include <expected>
#include <filesystem>
#include <variant>

namespace lod::io {

// PLY 读取错误类型
enum class PlyError {
    FileNotFound,
    InvalidFormat,
    UnsupportedFormat,
    ReadError,
    EmptyMesh
};

// PLY 文件元数据
struct PlyMetadata {
    size_t vertexCount{0};
    size_t faceCount{0};
    bool hasNormals{false};
    bool hasColors{false};
    bool hasTexCoords{false};
    std::string format;  // ascii, binary_little_endian, binary_big_endian
};

// PLY 读取器接口
class IPlyReader {
public:
    virtual ~IPlyReader() = default;
    
    // 读取 PLY 文件
    virtual std::expected<core::Mesh, PlyError> 
    readPly(const std::filesystem::path& filePath) const = 0;
    
    // 读取元数据（不加载完整网格）
    virtual std::expected<PlyMetadata, PlyError> 
    readMetadata(const std::filesystem::path& filePath) const = 0;
    
    // 批量读取
    virtual std::expected<std::vector<core::Mesh>, PlyError>
    readMultiple(const std::vector<std::filesystem::path>& filePaths) const = 0;
};

// 标准 PLY 读取器实现
class StandardPlyReader : public IPlyReader {
public:
    StandardPlyReader() = default;
    
    std::expected<core::Mesh, PlyError> 
    readPly(const std::filesystem::path& filePath) const override;
    
    std::expected<PlyMetadata, PlyError> 
    readMetadata(const std::filesystem::path& filePath) const override;
    
    std::expected<std::vector<core::Mesh>, PlyError>
    readMultiple(const std::vector<std::filesystem::path>& filePaths) const override;

private:
    // 解析 PLY 头部
    std::expected<PlyMetadata, PlyError> 
    parseHeader(std::istream& stream) const;
    
    // 读取顶点数据
    std::expected<core::VertexAttributes, PlyError>
    readVertices(std::istream& stream, const PlyMetadata& metadata) const;
    
    // 读取面片数据
    std::expected<std::vector<core::Index>, PlyError>
    readFaces(std::istream& stream, const PlyMetadata& metadata) const;
};

// PLY 文件信息结构（带地理信息）
struct PlyFileInfo {
    std::filesystem::path filePath;
    geo::GeoPoint origin;  // 文件的地理原点
    std::optional<std::string> crsCode;  // 坐标系代码，如 "EPSG:4326"
};

// 简单 PLY 文件信息（纯几何）
struct SimplePlyFileInfo {
    std::filesystem::path filePath;
    std::optional<std::array<float, 3>> offset;  // 可选的坐标偏移
};

// 输入配置的变体类型
using InputConfig = std::variant<
    std::filesystem::path,                    // 单个 PLY 文件
    std::vector<std::filesystem::path>,       // 多个 PLY 文件（纯几何）
    std::vector<PlyFileInfo>,                 // 带地理信息的文件列表
    std::vector<SimplePlyFileInfo>            // 带偏移的文件列表
>;

// 带地理信息的 PLY 读取器
class GeoPlyReader : public IPlyReader {
public:
    explicit GeoPlyReader(std::vector<PlyFileInfo> fileInfos)
        : fileInfos_(std::move(fileInfos)) {}
    
    std::expected<core::Mesh, PlyError> 
    readPly(const std::filesystem::path& filePath) const override;
    
    std::expected<PlyMetadata, PlyError> 
    readMetadata(const std::filesystem::path& filePath) const override;
    
    std::expected<std::vector<core::Mesh>, PlyError>
    readMultiple(const std::vector<std::filesystem::path>& filePaths) const override;
    
    // 读取所有文件并合并到统一坐标系
    std::expected<std::pair<core::Mesh, geo::GeoBBox>, PlyError>
    readAllWithGeoBounds() const;

private:
    std::vector<PlyFileInfo> fileInfos_;
    StandardPlyReader standardReader_;
    
    // 查找文件对应的地理信息
    std::optional<PlyFileInfo> findFileInfo(const std::filesystem::path& path) const;
};

// 几何 PLY 读取器（纯几何，支持偏移）
class GeometricPlyReader : public IPlyReader {
public:
    explicit GeometricPlyReader(std::vector<SimplePlyFileInfo> fileInfos)
        : fileInfos_(std::move(fileInfos)) {}
    
    std::expected<core::Mesh, PlyError> 
    readPly(const std::filesystem::path& filePath) const override;
    
    std::expected<PlyMetadata, PlyError> 
    readMetadata(const std::filesystem::path& filePath) const override;
    
    std::expected<std::vector<core::Mesh>, PlyError>
    readMultiple(const std::vector<std::filesystem::path>& filePaths) const override;
    
    // 读取所有文件并合并（纯几何）
    std::expected<std::pair<core::Mesh, core::BoundingBox>, PlyError>
    readAllWithBounds() const;

private:
    std::vector<SimplePlyFileInfo> fileInfos_;
    StandardPlyReader standardReader_;
    
    // 查找文件对应的偏移信息
    std::optional<SimplePlyFileInfo> findFileInfo(const std::filesystem::path& path) const;
    
    // 应用偏移到网格
    core::Mesh applyOffset(const core::Mesh& mesh, const std::array<float, 3>& offset) const;
};

// 通用输入读取器（支持所有输入类型）
class UniversalPlyReader {
public:
    explicit UniversalPlyReader(InputConfig config) : config_(std::move(config)) {}
    
    // 读取输入并返回结果
    // 返回类型：(Mesh, BoundingInfo) 其中 BoundingInfo 可能是 GeoBBox 或 BoundingBox
    std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError>
    readInput() const;
    
    // 判断是否为地理坐标模式
    bool isGeographicMode() const;

private:
    InputConfig config_;
    
    // 处理不同类型的输入
    std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError>
    processSingleFile(const std::filesystem::path& path) const;
    
    std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError>
    processMultipleFiles(const std::vector<std::filesystem::path>& paths) const;
    
    std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError>
    processGeoFiles(const std::vector<PlyFileInfo>& fileInfos) const;
    
    std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError>
    processSimpleFiles(const std::vector<SimplePlyFileInfo>& fileInfos) const;
};

// 工厂函数
[[nodiscard]] std::unique_ptr<IPlyReader> createPlyReader();
[[nodiscard]] std::unique_ptr<GeoPlyReader> createGeoPlyReader(std::vector<PlyFileInfo> fileInfos);
[[nodiscard]] std::unique_ptr<GeometricPlyReader> createGeometricPlyReader(std::vector<SimplePlyFileInfo> fileInfos);
[[nodiscard]] std::unique_ptr<UniversalPlyReader> createUniversalPlyReader(InputConfig config);

// 辅助函数：从文本文件读取 PLY 文件列表（地理模式）
[[nodiscard]] std::expected<std::vector<PlyFileInfo>, PlyError>
loadPlyFileList(const std::filesystem::path& listFile);

// 辅助函数：从文本文件读取 PLY 文件列表（几何模式）
[[nodiscard]] std::expected<std::vector<SimplePlyFileInfo>, PlyError>
loadSimplePlyFileList(const std::filesystem::path& listFile);

// 辅助函数：自动检测输入类型并创建配置
[[nodiscard]] std::expected<InputConfig, PlyError>
autoDetectInputConfig(const std::string& input);

} // namespace lod::io 