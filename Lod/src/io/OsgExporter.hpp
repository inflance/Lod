#pragma once

#include "../core/LodAlgorithm.hpp"
#include <string>
#include <filesystem>
#include <expected>

namespace lod::io {

// OSG 导出错误类型
enum class OsgError {
    WriteError,
    InvalidPath,
    UnsupportedFormat,
    ConversionError
};

// OSG 导出配置
struct OsgExportConfig {
    bool enableCompression{true};
    bool generateTextures{false};
    std::string textureFormat{"jpg"};  // jpg, png, dds
    bool optimizeGeometry{true};
    bool mergeGeometry{true};
    int compressionLevel{6};  // 0-9
};

// OSG 导出器接口
class IOsgExporter {
public:
    virtual ~IOsgExporter() = default;
    
    // 导出单个 LOD 节点
    virtual std::expected<void, OsgError>
    exportNode(const core::LodNode& node, const std::filesystem::path& outputPath) const = 0;
    
    // 导出完整 LOD 层次结构
    virtual std::expected<void, OsgError>
    exportHierarchy(const core::LodNode& root, const std::filesystem::path& outputDir) const = 0;
    
    // 导出为单个 OSGB 文件
    virtual std::expected<void, OsgError>
    exportSingleFile(const core::LodNode& root, const std::filesystem::path& outputFile) const = 0;
};

// 标准 OSG 导出器实现
class StandardOsgExporter : public IOsgExporter {
public:
    explicit StandardOsgExporter(OsgExportConfig config = {})
        : config_(std::move(config)) {}
    
    std::expected<void, OsgError>
    exportNode(const core::LodNode& node, const std::filesystem::path& outputPath) const override;
    
    std::expected<void, OsgError>
    exportHierarchy(const core::LodNode& root, const std::filesystem::path& outputDir) const override;
    
    std::expected<void, OsgError>
    exportSingleFile(const core::LodNode& root, const std::filesystem::path& outputFile) const override;

private:
    OsgExportConfig config_;
    
    // 转换网格到 OSG 几何体
    osg::ref_ptr<osg::Geometry> meshToGeometry(const core::Mesh& mesh) const;
    
    // 创建 LOD 节点
    osg::ref_ptr<osg::LOD> createLodNode(const core::LodNode& lodNode) const;
    
    // 应用优化
    void optimizeNode(osg::Node* node) const;
    
    // 生成文件名
    std::string generateFileName(const core::LodNode& node, const std::string& extension) const;
};

// 分层导出器（每个 LOD 级别一个文件）
class HierarchicalOsgExporter : public IOsgExporter {
public:
    explicit HierarchicalOsgExporter(OsgExportConfig config = {})
        : config_(std::move(config)) {}
    
    std::expected<void, OsgError>
    exportNode(const core::LodNode& node, const std::filesystem::path& outputPath) const override;
    
    std::expected<void, OsgError>
    exportHierarchy(const core::LodNode& root, const std::filesystem::path& outputDir) const override;
    
    std::expected<void, OsgError>
    exportSingleFile(const core::LodNode& root, const std::filesystem::path& outputFile) const override;

private:
    OsgExportConfig config_;
    StandardOsgExporter standardExporter_;
    
    // 创建目录结构
    void createDirectoryStructure(const std::filesystem::path& baseDir, const core::LodNode& root) const;
    
    // 导出级别
    std::expected<void, OsgError>
    exportLevel(const std::vector<std::reference_wrapper<const core::LodNode>>& nodes,
                int level, const std::filesystem::path& outputDir) const;
};

// 工厂函数
[[nodiscard]] std::unique_ptr<IOsgExporter> 
createOsgExporter(const OsgExportConfig& config = {});

[[nodiscard]] std::unique_ptr<HierarchicalOsgExporter> 
createHierarchicalOsgExporter(const OsgExportConfig& config = {});

// 辅助函数：验证 OSG 环境
[[nodiscard]] bool isOsgAvailable() noexcept;

// 辅助函数：获取支持的格式列表
[[nodiscard]] std::vector<std::string> getSupportedOsgFormats() noexcept;

} // namespace lod::io 