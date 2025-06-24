#include "PlyReader.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <execution>
#include <cctype>

namespace lod::io {

// StandardPlyReader 实现
std::expected<core::Mesh, PlyError> StandardPlyReader::readPly(const std::filesystem::path& filePath) const {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(PlyError::FileNotFound);
    }
    
    // 解析头部
    auto metadataResult = parseHeader(file);
    if (!metadataResult) {
        return std::unexpected(metadataResult.error());
    }
    
    const auto& metadata = metadataResult.value();
    
    // 读取顶点数据
    auto verticesResult = readVertices(file, metadata);
    if (!verticesResult) {
        return std::unexpected(verticesResult.error());
    }
    
    // 读取面片数据
    auto facesResult = readFaces(file, metadata);
    if (!facesResult) {
        return std::unexpected(facesResult.error());
    }
    
    core::Mesh mesh{std::move(verticesResult.value()), std::move(facesResult.value())};
    
    if (mesh.empty()) {
        return std::unexpected(PlyError::EmptyMesh);
    }
    
    return mesh;
}

std::expected<PlyMetadata, PlyError> StandardPlyReader::readMetadata(const std::filesystem::path& filePath) const {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(PlyError::FileNotFound);
    }
    
    return parseHeader(file);
}

std::expected<std::vector<core::Mesh>, PlyError> StandardPlyReader::readMultiple(const std::vector<std::filesystem::path>& filePaths) const {
    std::vector<core::Mesh> meshes;
    meshes.reserve(filePaths.size());
    
    for (const auto& path : filePaths) {
        auto result = readPly(path);
        if (!result) {
            return std::unexpected(result.error());
        }
        meshes.push_back(std::move(result.value()));
    }
    
    return meshes;
}

std::expected<PlyMetadata, PlyError> StandardPlyReader::parseHeader(std::istream& stream) const {
    PlyMetadata metadata;
    std::string line;
    
    // 读取第一行，应该是 "ply"
    if (!std::getline(stream, line) || line != "ply") {
        return std::unexpected(PlyError::InvalidFormat);
    }
    
    while (std::getline(stream, line)) {
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;
        
        if (keyword == "format") {
            iss >> metadata.format;
        } else if (keyword == "element") {
            std::string elementType;
            size_t count;
            iss >> elementType >> count;
            
            if (elementType == "vertex") {
                metadata.vertexCount = count;
            } else if (elementType == "face") {
                metadata.faceCount = count;
            }
        } else if (keyword == "property") {
            std::string type, name;
            iss >> type >> name;
            
            if (name == "nx" || name == "ny" || name == "nz") {
                metadata.hasNormals = true;
            } else if (name == "red" || name == "green" || name == "blue" || name == "alpha") {
                metadata.hasColors = true;
            } else if (name == "u" || name == "v" || name == "s" || name == "t") {
                metadata.hasTexCoords = true;
            }
        } else if (keyword == "end_header") {
            break;
        }
    }
    
    return metadata;
}

std::expected<core::VertexAttributes, PlyError> StandardPlyReader::readVertices(std::istream& stream, const PlyMetadata& metadata) const {
    core::VertexAttributes vertices;
    vertices.reserve(metadata.vertexCount);
    
    if (metadata.format == "ascii") {
        // ASCII 格式读取
        for (size_t i = 0; i < metadata.vertexCount; ++i) {
            std::string line;
            if (!std::getline(stream, line)) {
                return std::unexpected(PlyError::ReadError);
            }
            
            std::istringstream iss(line);
            float x, y, z;
            iss >> x >> y >> z;
            vertices.positions.push_back({x, y, z});
            
            // 读取法线（如果存在）
            if (metadata.hasNormals) {
                float nx, ny, nz;
                iss >> nx >> ny >> nz;
                vertices.normals.push_back({nx, ny, nz});
            }
            
            // 读取颜色（如果存在）
            if (metadata.hasColors) {
                int r, g, b, a = 255;
                iss >> r >> g >> b;
                if (!(iss >> a)) {
                    a = 255; // 默认不透明
                }
                vertices.colors.push_back({static_cast<uint8_t>(r), static_cast<uint8_t>(g), 
                                         static_cast<uint8_t>(b), static_cast<uint8_t>(a)});
            }
            
            // 读取纹理坐标（如果存在）
            if (metadata.hasTexCoords) {
                float u, v;
                iss >> u >> v;
                vertices.texCoords.push_back({u, v});
            }
        }
    } else if (metadata.format.find("binary") != std::string::npos) {
        // 二进制格式读取（简化实现）
        for (size_t i = 0; i < metadata.vertexCount; ++i) {
            float pos[3];
            stream.read(reinterpret_cast<char*>(pos), sizeof(pos));
            vertices.positions.push_back({pos[0], pos[1], pos[2]});
            
            // 这里应该根据实际的二进制格式读取其他属性
            // 当前只实现基本的位置读取
        }
    } else {
        return std::unexpected(PlyError::UnsupportedFormat);
    }
    
    return vertices;
}

std::expected<std::vector<core::Index>, PlyError> StandardPlyReader::readFaces(std::istream& stream, const PlyMetadata& metadata) const {
    std::vector<core::Index> indices;
    indices.reserve(metadata.faceCount * 3); // 假设大部分是三角形
    
    if (metadata.format == "ascii") {
        for (size_t i = 0; i < metadata.faceCount; ++i) {
            std::string line;
            if (!std::getline(stream, line)) {
                return std::unexpected(PlyError::ReadError);
            }
            
            std::istringstream iss(line);
            int vertexCount;
            iss >> vertexCount;
            
            std::vector<core::Index> faceIndices(vertexCount);
            for (int j = 0; j < vertexCount; ++j) {
                iss >> faceIndices[j];
            }
            
            // 三角化（如果不是三角形）
            if (vertexCount == 3) {
                indices.insert(indices.end(), faceIndices.begin(), faceIndices.end());
            } else if (vertexCount > 3) {
                // 简单的扇形三角化
                for (int j = 1; j < vertexCount - 1; ++j) {
                    indices.push_back(faceIndices[0]);
                    indices.push_back(faceIndices[j]);
                    indices.push_back(faceIndices[j + 1]);
                }
            }
        }
    } else if (metadata.format.find("binary") != std::string::npos) {
        // 二进制格式读取（简化实现）
        for (size_t i = 0; i < metadata.faceCount; ++i) {
            uint8_t vertexCount;
            stream.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
            
            std::vector<uint32_t> faceIndices(vertexCount);
            stream.read(reinterpret_cast<char*>(faceIndices.data()), vertexCount * sizeof(uint32_t));
            
            // 三角化
            if (vertexCount == 3) {
                indices.insert(indices.end(), faceIndices.begin(), faceIndices.end());
            } else if (vertexCount > 3) {
                for (int j = 1; j < vertexCount - 1; ++j) {
                    indices.push_back(faceIndices[0]);
                    indices.push_back(faceIndices[j]);
                    indices.push_back(faceIndices[j + 1]);
                }
            }
        }
    } else {
        return std::unexpected(PlyError::UnsupportedFormat);
    }
    
    return indices;
}

// GeoPlyReader 实现
std::expected<core::Mesh, PlyError> GeoPlyReader::readPly(const std::filesystem::path& filePath) const {
    return standardReader_.readPly(filePath);
}

std::expected<PlyMetadata, PlyError> GeoPlyReader::readMetadata(const std::filesystem::path& filePath) const {
    return standardReader_.readMetadata(filePath);
}

std::expected<std::vector<core::Mesh>, PlyError> GeoPlyReader::readMultiple(const std::vector<std::filesystem::path>& filePaths) const {
    return standardReader_.readMultiple(filePaths);
}

std::expected<std::pair<core::Mesh, geo::GeoBBox>, PlyError> GeoPlyReader::readAllWithGeoBounds() const {
    std::vector<core::Mesh> meshes;
    geo::GeoBBox totalBounds;
    bool firstBounds = true;
    
    for (const auto& fileInfo : fileInfos_) {
        auto meshResult = standardReader_.readPly(fileInfo.filePath);
        if (!meshResult) {
            return std::unexpected(meshResult.error());
        }
        
        meshes.push_back(std::move(meshResult.value()));
        
        // 更新总边界框（简化实现）
        // 这里应该根据文件的地理信息计算边界框
        geo::GeoBBox fileBounds{
            fileInfo.origin.longitude - 0.001,
            fileInfo.origin.latitude - 0.001,
            fileInfo.origin.longitude + 0.001,
            fileInfo.origin.latitude + 0.001
        };
        
        if (firstBounds) {
            totalBounds = fileBounds;
            firstBounds = false;
        } else {
            totalBounds = totalBounds.unite(fileBounds);
        }
    }
    
    // 合并所有网格
    auto mergedMesh = core::Mesh::merge(meshes);
    return std::make_pair(std::move(mergedMesh), totalBounds);
}

std::optional<PlyFileInfo> GeoPlyReader::findFileInfo(const std::filesystem::path& path) const {
    auto it = std::find_if(fileInfos_.begin(), fileInfos_.end(),
                          [&path](const PlyFileInfo& info) {
                              return info.filePath == path;
                          });
    
    return (it != fileInfos_.end()) ? std::make_optional(*it) : std::nullopt;
}

// GeometricPlyReader 实现
std::expected<core::Mesh, PlyError> GeometricPlyReader::readPly(const std::filesystem::path& filePath) const {
    auto meshResult = standardReader_.readPly(filePath);
    if (!meshResult) {
        return meshResult;
    }
    
    // 应用偏移（如果存在）
    auto fileInfo = findFileInfo(filePath);
    if (fileInfo && fileInfo->offset) {
        return applyOffset(meshResult.value(), *fileInfo->offset);
    }
    
    return meshResult;
}

std::expected<PlyMetadata, PlyError> GeometricPlyReader::readMetadata(const std::filesystem::path& filePath) const {
    return standardReader_.readMetadata(filePath);
}

std::expected<std::vector<core::Mesh>, PlyError> GeometricPlyReader::readMultiple(const std::vector<std::filesystem::path>& filePaths) const {
    std::vector<core::Mesh> meshes;
    meshes.reserve(filePaths.size());
    
    for (const auto& path : filePaths) {
        auto result = readPly(path);
        if (!result) {
            return std::unexpected(result.error());
        }
        meshes.push_back(std::move(result.value()));
    }
    
    return meshes;
}

std::expected<std::pair<core::Mesh, core::BoundingBox>, PlyError> GeometricPlyReader::readAllWithBounds() const {
    std::vector<core::Mesh> meshes;
    
    for (const auto& fileInfo : fileInfos_) {
        auto meshResult = readPly(fileInfo.filePath);
        if (!meshResult) {
            return std::unexpected(meshResult.error());
        }
        meshes.push_back(std::move(meshResult.value()));
    }
    
    // 合并所有网格
    auto mergedMesh = core::Mesh::merge(meshes);
    auto bounds = core::computeBoundingBox(mergedMesh);
    
    return std::make_pair(std::move(mergedMesh), bounds);
}

std::optional<SimplePlyFileInfo> GeometricPlyReader::findFileInfo(const std::filesystem::path& path) const {
    auto it = std::find_if(fileInfos_.begin(), fileInfos_.end(),
                          [&path](const SimplePlyFileInfo& info) {
                              return info.filePath == path;
                          });
    
    return (it != fileInfos_.end()) ? std::make_optional(*it) : std::nullopt;
}

core::Mesh GeometricPlyReader::applyOffset(const core::Mesh& mesh, const std::array<float, 3>& offset) const {
    auto vertices = mesh.vertices();
    
    // 应用偏移到所有顶点位置
    for (auto& pos : vertices.positions) {
        pos[0] += offset[0];
        pos[1] += offset[1];
        pos[2] += offset[2];
    }
    
    return core::Mesh{std::move(vertices), mesh.indices()};
}

// UniversalPlyReader 实现
std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError> 
UniversalPlyReader::readInput() const {
    return std::visit([this](const auto& config) -> std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError> {
        using T = std::decay_t<decltype(config)>;
        
        if constexpr (std::is_same_v<T, std::filesystem::path>) {
            return processSingleFile(config);
        } else if constexpr (std::is_same_v<T, std::vector<std::filesystem::path>>) {
            return processMultipleFiles(config);
        } else if constexpr (std::is_same_v<T, std::vector<PlyFileInfo>>) {
            return processGeoFiles(config);
        } else if constexpr (std::is_same_v<T, std::vector<SimplePlyFileInfo>>) {
            return processSimpleFiles(config);
        }
    }, config_);
}

bool UniversalPlyReader::isGeographicMode() const {
    return std::visit([](const auto& config) -> bool {
        using T = std::decay_t<decltype(config)>;
        return std::is_same_v<T, std::vector<PlyFileInfo>>;
    }, config_);
}

std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError>
UniversalPlyReader::processSingleFile(const std::filesystem::path& path) const {
    StandardPlyReader reader;
    auto meshResult = reader.readPly(path);
    if (!meshResult) {
        return std::unexpected(meshResult.error());
    }
    
    auto bounds = core::computeBoundingBox(meshResult.value());
    return std::make_pair(std::move(meshResult.value()), bounds);
}

std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError>
UniversalPlyReader::processMultipleFiles(const std::vector<std::filesystem::path>& paths) const {
    GeometricPlyReader reader{{paths.begin(), paths.end()}};
    return reader.readAllWithBounds();
}

std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError>
UniversalPlyReader::processGeoFiles(const std::vector<PlyFileInfo>& fileInfos) const {
    GeoPlyReader reader{fileInfos};
    auto result = reader.readAllWithGeoBounds();
    if (!result) {
        return std::unexpected(result.error());
    }
    
    return std::make_pair(std::move(result.value().first), result.value().second);
}

std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PlyError>
UniversalPlyReader::processSimpleFiles(const std::vector<SimplePlyFileInfo>& fileInfos) const {
    GeometricPlyReader reader{fileInfos};
    return reader.readAllWithBounds();
}

// 工厂函数实现
std::unique_ptr<IPlyReader> createPlyReader() {
    return std::make_unique<StandardPlyReader>();
}

std::unique_ptr<GeoPlyReader> createGeoPlyReader(std::vector<PlyFileInfo> fileInfos) {
    return std::make_unique<GeoPlyReader>(std::move(fileInfos));
}

std::unique_ptr<GeometricPlyReader> createGeometricPlyReader(std::vector<SimplePlyFileInfo> fileInfos) {
    return std::make_unique<GeometricPlyReader>(std::move(fileInfos));
}

std::unique_ptr<UniversalPlyReader> createUniversalPlyReader(InputConfig config) {
    return std::make_unique<UniversalPlyReader>(std::move(config));
}

// 辅助函数实现
std::expected<std::vector<PlyFileInfo>, PlyError> loadPlyFileList(const std::filesystem::path& listFile) {
    std::ifstream file(listFile);
    if (!file.is_open()) {
        return std::unexpected(PlyError::FileNotFound);
    }
    
    std::vector<PlyFileInfo> fileInfos;
    std::string line;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string filePath;
        double lon, lat, alt = 0.0;
        std::string crs = "EPSG:4326";
        
        iss >> filePath >> lon >> lat >> alt >> crs;
        
        fileInfos.push_back({
            std::filesystem::path(filePath),
            geo::GeoPoint{lon, lat, alt},
            crs
        });
    }
    
    return fileInfos;
}

std::expected<std::vector<SimplePlyFileInfo>, PlyError> loadSimplePlyFileList(const std::filesystem::path& listFile) {
    std::ifstream file(listFile);
    if (!file.is_open()) {
        return std::unexpected(PlyError::FileNotFound);
    }
    
    std::vector<SimplePlyFileInfo> fileInfos;
    std::string line;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string filePath;
        
        SimplePlyFileInfo info;
        info.filePath = std::filesystem::path(filePath);
        
        float x, y, z;
        if (iss >> x >> y >> z) {
            info.offset = std::array<float, 3>{x, y, z};
        }
        
        fileInfos.push_back(std::move(info));
    }
    
    return fileInfos;
}

std::expected<InputConfig, PlyError> autoDetectInputConfig(const std::string& input) {
    std::filesystem::path path(input);
    
    // 检查是否是单个文件
    if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
        if (path.extension() == ".ply") {
            return path;
        } else {
            // 可能是列表文件
            auto geoResult = loadPlyFileList(path);
            if (geoResult) {
                return geoResult.value();
            }
            
            auto simpleResult = loadSimplePlyFileList(path);
            if (simpleResult) {
                return simpleResult.value();
            }
        }
    }
    
    return std::unexpected(PlyError::InvalidFormat);
}

} // namespace lod::io 