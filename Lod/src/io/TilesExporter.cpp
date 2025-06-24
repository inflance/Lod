#include "TilesExporter.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace lod::io {

// TilesetBuilder 实现
nlohmann::json TilesetBuilder::buildTileset(const core::LodNode& root) const {
    nlohmann::json tileset;
    
    // Asset 信息
    tileset["asset"] = buildAsset();
    
    // 几何误差
    tileset["geometricError"] = std::visit([this](const auto& node) -> double {
        return calculateGeometricError(node);
    }, root);
    
    // 根瓦片
    tileset["root"] = buildTile(root);
    
    return tileset;
}

nlohmann::json TilesetBuilder::buildTile(const core::LodNode& node, const std::string& contentUri) const {
    nlohmann::json tile;
    
    return std::visit([&](const auto& lodNode) -> nlohmann::json {
        // 几何误差
        tile["geometricError"] = lodNode.geometricError;
        
        // 包围体
        if constexpr (std::is_same_v<std::decay_t<decltype(lodNode)>, core::GeoLodNode>) {
            tile["boundingVolume"] = buildBoundingVolume(lodNode.region);
        } else {
            // 几何模式：转换为地理包围盒（简化）
            geo::GeoBBox region{
                static_cast<double>(lodNode.bounds.min[0]),
                static_cast<double>(lodNode.bounds.min[1]),
                static_cast<double>(lodNode.bounds.max[0]),
                static_cast<double>(lodNode.bounds.max[1])
            };
            tile["boundingVolume"] = buildBoundingVolume(region);
        }
        
        // 细分条件
        tile["refine"] = "REPLACE";
        
        // 内容（如果有网格数据）
        if (!lodNode.mesh.empty()) {
            std::string uri = contentUri.empty() ? generateContentUri(lodNode) : contentUri;
            tile["content"] = nlohmann::json{{"uri", uri}};
        }
        
        // 子瓦片
        if (!lodNode.children.empty()) {
            nlohmann::json children = nlohmann::json::array();
            
            for (const auto& child : lodNode.children) {
                if (child) {
                    children.push_back(buildTile(*child));
                }
            }
            
            if (!children.empty()) {
                tile["children"] = children;
            }
        }
        
        return tile;
    }, node);
}

nlohmann::json TilesetBuilder::buildAsset() const {
    nlohmann::json asset;
    asset["version"] = config_.asset_version;
    asset["generator"] = "LOD Generator";
    
    if (config_.copyright) {
        asset["copyright"] = *config_.copyright;
    }
    
    return asset;
}

nlohmann::json TilesetBuilder::buildBoundingVolume(const geo::GeoBBox& region) const {
    nlohmann::json boundingVolume;
    
    // 使用地理边界框（度）
    boundingVolume["region"] = nlohmann::json::array({
        region.minLon * M_PI / 180.0,  // 西边界（弧度）
        region.minLat * M_PI / 180.0,  // 南边界（弧度）
        region.maxLon * M_PI / 180.0,  // 东边界（弧度）
        region.maxLat * M_PI / 180.0,  // 北边界（弧度）
        0.0,    // 最小高度
        1000.0  // 最大高度（暂定）
    });
    
    return boundingVolume;
}

void TilesetBuilder::buildChildTiles(nlohmann::json& parentTile, const core::LodNode& node) const {
    std::visit([&](const auto& lodNode) {
        if (!lodNode.children.empty()) {
            nlohmann::json children = nlohmann::json::array();
            
            for (const auto& child : lodNode.children) {
                if (child) {
                    children.push_back(buildTile(*child));
                }
            }
            
            parentTile["children"] = children;
        }
    }, node);
}

double TilesetBuilder::calculateGeometricError(const core::LodNode& node) const {
    return std::visit([](const auto& lodNode) -> double {
        return lodNode.geometricError > 0.0 ? lodNode.geometricError : 100.0;
    }, node);
}

std::string TilesetBuilder::generateContentUri(const core::LodNode& node) const {
    return std::visit([](const auto& lodNode) -> std::string {
        return "tiles/level_" + std::to_string(lodNode.lodLevel) + 
               "_" + std::to_string(reinterpret_cast<uintptr_t>(&lodNode)) + ".b3dm";
    }, node);
}

// B3dmExporter 实现
std::expected<void, TilesError> B3dmExporter::exportTileset(const core::LodNode& root, const std::filesystem::path& outputDir) const {
    // 创建输出目录结构
    createDirectoryStructure(outputDir, root);
    
    // 导出 tileset.json
    auto tilesetResult = generateTilesetJson(root, outputDir / "tileset.json");
    if (!tilesetResult) {
        return tilesetResult;
    }
    
    // 递归导出所有瓦片内容
    std::function<std::expected<void, TilesError>(const core::LodNode&, const std::filesystem::path&)> exportRecursive;
    
    exportRecursive = [&](const core::LodNode& node, const std::filesystem::path& basePath) -> std::expected<void, TilesError> {
        return std::visit([&](const auto& lodNode) -> std::expected<void, TilesError> {
            // 导出当前节点的内容
            if (!lodNode.mesh.empty()) {
                std::string filename = "level_" + std::to_string(lodNode.lodLevel) + 
                                     "_" + std::to_string(reinterpret_cast<uintptr_t>(&lodNode)) + ".b3dm";
                auto filePath = basePath / "tiles" / filename;
                
                auto result = exportTileContent(node, filePath);
                if (!result) {
                    return result;
                }
            }
            
            // 递归导出子节点
            for (const auto& child : lodNode.children) {
                if (child) {
                    auto childResult = exportRecursive(*child, basePath);
                    if (!childResult) {
                        return childResult;
                    }
                }
            }
            
            return {};
        }, node);
    };
    
    return exportRecursive(root, outputDir);
}

std::expected<void, TilesError> B3dmExporter::exportTileContent(const core::LodNode& node, const std::filesystem::path& outputFile) const {
    return std::visit([&](const auto& lodNode) -> std::expected<void, TilesError> {
        // 创建 GLB 内容
        auto glbResult = createGlbContent(lodNode.mesh);
        if (!glbResult) {
            return std::unexpected(glbResult.error());
        }
        
        // 创建 B3DM 文件
        auto b3dmResult = createB3dmFile(glbResult.value());
        if (!b3dmResult) {
            return std::unexpected(b3dmResult.error());
        }
        
        // 写入文件
        std::ofstream file(outputFile, std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(TilesError::WriteError);
        }
        
        const auto& data = b3dmResult.value();
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        
        if (!file.good()) {
            return std::unexpected(TilesError::WriteError);
        }
        
        return {};
    }, node);
}

std::expected<void, TilesError> B3dmExporter::generateTilesetJson(const core::LodNode& root, const std::filesystem::path& outputFile) const {
    auto tileset = tilesetBuilder_.buildTileset(root);
    
    std::ofstream file(outputFile);
    if (!file.is_open()) {
        return std::unexpected(TilesError::WriteError);
    }
    
    try {
        file << std::setw(2) << tileset << std::endl;
    } catch (const nlohmann::json::exception&) {
        return std::unexpected(TilesError::JsonError);
    }
    
    return {};
}

std::expected<std::vector<uint8_t>, TilesError> B3dmExporter::createGlbContent(const core::Mesh& mesh) const {
    // 简化的 GLB 创建（实际实现需要使用 tinygltf 库）
    std::vector<uint8_t> glbData;
    
    try {
        // 这里应该使用 tinygltf 库来创建 GLB 文件
        // 当前是占位符实现
        
        // GLB 头部（12字节）
        glbData.resize(12);
        
        // Magic number "glTF"
        glbData[0] = 'g'; glbData[1] = 'l'; glbData[2] = 'T'; glbData[3] = 'F';
        
        // Version (2)
        uint32_t version = 2;
        std::memcpy(&glbData[4], &version, 4);
        
        // Total length (will be updated)
        uint32_t totalLength = 12;
        std::memcpy(&glbData[8], &totalLength, 4);
        
        // 这里应该添加实际的网格数据转换
        // 当前返回最小的有效 GLB
        
        return glbData;
    } catch (const std::exception&) {
        return std::unexpected(TilesError::GlbError);
    }
}

std::expected<std::vector<uint8_t>, TilesError> B3dmExporter::createB3dmFile(const std::vector<uint8_t>& glbContent) const {
    try {
        std::vector<uint8_t> b3dmData;
        
        // B3DM 头部
        const uint32_t headerLength = 28;
        const uint32_t featureTableJsonLength = 0;
        const uint32_t featureTableBinaryLength = 0;
        const uint32_t batchTableJsonLength = 0;
        const uint32_t batchTableBinaryLength = 0;
        
        const uint32_t totalLength = headerLength + 
                                   featureTableJsonLength + featureTableBinaryLength +
                                   batchTableJsonLength + batchTableBinaryLength +
                                   static_cast<uint32_t>(glbContent.size());
        
        b3dmData.resize(totalLength);
        
        size_t offset = 0;
        
        // Magic number "b3dm"
        b3dmData[offset++] = 'b'; b3dmData[offset++] = '3'; 
        b3dmData[offset++] = 'd'; b3dmData[offset++] = 'm';
        
        // Version (1)
        uint32_t version = 1;
        std::memcpy(&b3dmData[offset], &version, 4); offset += 4;
        
        // Byte length
        std::memcpy(&b3dmData[offset], &totalLength, 4); offset += 4;
        
        // Feature table JSON byte length
        std::memcpy(&b3dmData[offset], &featureTableJsonLength, 4); offset += 4;
        
        // Feature table binary byte length
        std::memcpy(&b3dmData[offset], &featureTableBinaryLength, 4); offset += 4;
        
        // Batch table JSON byte length
        std::memcpy(&b3dmData[offset], &batchTableJsonLength, 4); offset += 4;
        
        // Batch table binary byte length
        std::memcpy(&b3dmData[offset], &batchTableBinaryLength, 4); offset += 4;
        
        // GLB content
        std::memcpy(&b3dmData[offset], glbContent.data(), glbContent.size());
        
        return b3dmData;
    } catch (const std::exception&) {
        return std::unexpected(TilesError::WriteError);
    }
}

std::expected<std::vector<uint8_t>, TilesError> B3dmExporter::applyDracoCompression(const std::vector<uint8_t>& glbContent) const {
    if (!config_.enableDracoCompression) {
        return glbContent;
    }
    
    // 这里应该使用 Draco 库进行压缩
    // 当前返回原始内容
    return glbContent;
}

void B3dmExporter::createDirectoryStructure(const std::filesystem::path& baseDir, const core::LodNode& root) const {
    std::filesystem::create_directories(baseDir);
    std::filesystem::create_directories(baseDir / "tiles");
}

// 工厂函数实现
std::unique_ptr<ITilesExporter> createTilesExporter(TileFormat format, const TilesExportConfig& config) {
    switch (format) {
        case TileFormat::B3DM:
            return std::make_unique<B3dmExporter>(config);
        default:
            return nullptr;
    }
}

std::unique_ptr<B3dmExporter> createB3dmExporter(const TilesExportConfig& config) {
    return std::make_unique<B3dmExporter>(config);
}

// 辅助函数实现
bool isDracoAvailable() noexcept {
    // 简单检查：这里应该检查 Draco 库是否可用
    return true; // 假设可用
}

bool isTinyGltfAvailable() noexcept {
    // 简单检查：这里应该检查 tinygltf 库是否可用
    return true; // 假设可用
}

std::expected<std::vector<uint8_t>, TilesError> meshToGlb(const core::Mesh& mesh, bool enableDraco) {
    // 这里应该使用 tinygltf 将网格转换为 GLB
    // 当前是占位符实现
    std::vector<uint8_t> glbData(12); // 最小 GLB 头部
    return glbData;
}

std::expected<std::vector<uint8_t>, TilesError> glbToB3dm(const std::vector<uint8_t>& glbData) {
    // 简化的 B3DM 包装
    std::vector<uint8_t> b3dmData;
    
    // B3DM 头部（28字节）
    b3dmData.resize(28 + glbData.size());
    
    // Magic "b3dm"
    b3dmData[0] = 'b'; b3dmData[1] = '3'; b3dmData[2] = 'd'; b3dmData[3] = 'm';
    
    // Version
    uint32_t version = 1;
    std::memcpy(&b3dmData[4], &version, 4);
    
    // Length
    uint32_t length = static_cast<uint32_t>(b3dmData.size());
    std::memcpy(&b3dmData[8], &length, 4);
    
    // 其他字段设为0
    std::memset(&b3dmData[12], 0, 16);
    
    // 复制 GLB 数据
    std::memcpy(&b3dmData[28], glbData.data(), glbData.size());
    
    return b3dmData;
}

std::array<double, 3> wgs84ToCartesian(double longitude, double latitude, double altitude) noexcept {
    const double a = 6378137.0; // WGS84 semi-major axis
    const double f = 1.0 / 298.257223563; // WGS84 flattening
    const double e2 = 2 * f - f * f; // First eccentricity squared
    
    const double lon_rad = longitude * M_PI / 180.0;
    const double lat_rad = latitude * M_PI / 180.0;
    
    const double N = a / std::sqrt(1 - e2 * std::sin(lat_rad) * std::sin(lat_rad));
    
    const double x = (N + altitude) * std::cos(lat_rad) * std::cos(lon_rad);
    const double y = (N + altitude) * std::cos(lat_rad) * std::sin(lon_rad);
    const double z = (N * (1 - e2) + altitude) * std::sin(lat_rad);
    
    return {x, y, z};
}

} // namespace lod::io 