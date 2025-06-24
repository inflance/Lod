#pragma once

#include "../core/LodAlgorithm.hpp"
#include <string>
#include <filesystem>
#include <expected>
#include <nlohmann/json.hpp>

namespace lod::io {

// 3D Tiles 导出错误类型
enum class TilesError {
    WriteError,
    InvalidPath,
    JsonError,
    GlbError,
    CompressionError
};

// 3D Tiles 格式类型
enum class TileFormat {
    B3DM,    // Batched 3D Model
    I3DM,    // Instanced 3D Model  
    PNTS,    // Point Cloud
    CMPT     // Composite
};

// 3D Tiles 导出配置
struct TilesExportConfig {
    TileFormat format{TileFormat::B3DM};
    bool enableDracoCompression{true};
    bool enableGzipCompression{false};
    int dracoCompressionLevel{7};  // 0-10
    bool generateNormals{true};
    bool optimizeForCesium{true};
    std::string asset_version{"1.1"};
    std::optional<std::string> copyright;
};

// Tileset.json 生成器
class TilesetBuilder {
public:
    explicit TilesetBuilder(TilesExportConfig config = {})
        : config_(std::move(config)) {}
    
    // 从 LOD 层次结构构建 tileset
    [[nodiscard]] nlohmann::json buildTileset(const core::LodNode& root) const;
    
    // 构建单个 tile 对象
    [[nodiscard]] nlohmann::json buildTile(const core::LodNode& node, 
                                          const std::string& contentUri = "") const;
    
    // 构建 asset 信息
    [[nodiscard]] nlohmann::json buildAsset() const;
    
    // 构建 bounding volume
    [[nodiscard]] nlohmann::json buildBoundingVolume(const geo::GeoBBox& region) const;

private:
    TilesExportConfig config_;
    
    // 递归构建子瓦片
    void buildChildTiles(nlohmann::json& parentTile, const core::LodNode& node) const;
    
    // 计算几何误差
    double calculateGeometricError(const core::LodNode& node) const;
    
    // 生成内容 URI
    std::string generateContentUri(const core::LodNode& node) const;
};

// 3D Tiles 导出器接口
class ITilesExporter {
public:
    virtual ~ITilesExporter() = default;
    
    // 导出完整的 3D Tiles 数据集
    virtual std::expected<void, TilesError>
    exportTileset(const core::LodNode& root, const std::filesystem::path& outputDir) const = 0;
    
    // 导出单个瓦片内容
    virtual std::expected<void, TilesError>
    exportTileContent(const core::LodNode& node, const std::filesystem::path& outputFile) const = 0;
    
    // 生成 tileset.json
    virtual std::expected<void, TilesError>
    generateTilesetJson(const core::LodNode& root, const std::filesystem::path& outputFile) const = 0;
};

// B3DM 导出器实现
class B3dmExporter : public ITilesExporter {
public:
    explicit B3dmExporter(TilesExportConfig config = {})
        : config_(std::move(config)), tilesetBuilder_(config) {}
    
    std::expected<void, TilesError>
    exportTileset(const core::LodNode& root, const std::filesystem::path& outputDir) const override;
    
    std::expected<void, TilesError>
    exportTileContent(const core::LodNode& node, const std::filesystem::path& outputFile) const override;
    
    std::expected<void, TilesError>
    generateTilesetJson(const core::LodNode& root, const std::filesystem::path& outputFile) const override;

private:
    TilesExportConfig config_;
    TilesetBuilder tilesetBuilder_;
    
    // 创建 GLB 内容
    std::expected<std::vector<uint8_t>, TilesError>
    createGlbContent(const core::Mesh& mesh) const;
    
    // 创建 B3DM 文件
    std::expected<std::vector<uint8_t>, TilesError>
    createB3dmFile(const std::vector<uint8_t>& glbContent) const;
    
    // 应用 Draco 压缩
    std::expected<std::vector<uint8_t>, TilesError>
    applyDracoCompression(const std::vector<uint8_t>& glbContent) const;
    
    // 创建目录结构
    void createDirectoryStructure(const std::filesystem::path& baseDir, const core::LodNode& root) const;
};

// 工厂函数
[[nodiscard]] std::unique_ptr<ITilesExporter> 
createTilesExporter(TileFormat format, const TilesExportConfig& config = {});

[[nodiscard]] std::unique_ptr<B3dmExporter> 
createB3dmExporter(const TilesExportConfig& config = {});

// 辅助函数：验证 3D Tiles 环境
[[nodiscard]] bool isDracoAvailable() noexcept;
[[nodiscard]] bool isTinyGltfAvailable() noexcept;

// 辅助函数：GLB 操作
[[nodiscard]] std::expected<std::vector<uint8_t>, TilesError>
meshToGlb(const core::Mesh& mesh, bool enableDraco = false);

[[nodiscard]] std::expected<std::vector<uint8_t>, TilesError>
glbToB3dm(const std::vector<uint8_t>& glbData);

// 辅助函数：坐标转换（WGS84 -> Cartesian）
[[nodiscard]] std::array<double, 3> 
wgs84ToCartesian(double longitude, double latitude, double altitude = 0.0) noexcept;

} // namespace lod::io 