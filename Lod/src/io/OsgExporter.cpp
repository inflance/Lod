#include "OsgExporter.hpp"
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/LOD>
#include <osgDB/WriteFile>
#include <osgUtil/Simplifier>
#include <osgUtil/Optimizer>

namespace lod::io {

// StandardOsgExporter 实现
std::expected<void, OsgError> StandardOsgExporter::exportNode(const core::LodNode& node, const std::filesystem::path& outputPath) const {
    try {
        auto osgNode = std::visit([this](const auto& lodNode) -> osg::ref_ptr<osg::Node> {
            return createLodNode(lodNode);
        }, node);
        
        if (config_.optimizeGeometry) {
            optimizeNode(osgNode);
        }
        
        bool success = osgDB::writeNodeFile(*osgNode, outputPath.string());
        if (!success) {
            return std::unexpected(OsgError::WriteError);
        }
        
        return {};
    } catch (const std::exception&) {
        return std::unexpected(OsgError::ConversionError);
    }
}

std::expected<void, OsgError> StandardOsgExporter::exportHierarchy(const core::LodNode& root, const std::filesystem::path& outputDir) const {
    if (!std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
    }
    
    // 递归导出每个节点
    auto exportRecursive = [&](const auto& node, const std::filesystem::path& basePath, int level) -> std::expected<void, OsgError> {
        auto nodePath = basePath / (std::string("level_") + std::to_string(level) + ".osgb");
        auto result = exportNode(node, nodePath);
        if (!result) {
            return result;
        }
        
        // 导出子节点
        return std::visit([&](const auto& lodNode) -> std::expected<void, OsgError> {
            for (const auto& child : lodNode.children) {
                if (child) {
                    auto childResult = exportRecursive(*child, basePath / ("level_" + std::to_string(level)), level + 1);
                    if (!childResult) {
                        return childResult;
                    }
                }
            }
            return {};
        }, node);
    };
    
    return exportRecursive(root, outputDir, 0);
}

std::expected<void, OsgError> StandardOsgExporter::exportSingleFile(const core::LodNode& root, const std::filesystem::path& outputFile) const {
    return exportNode(root, outputFile);
}

osg::ref_ptr<osg::Geometry> StandardOsgExporter::meshToGeometry(const core::Mesh& mesh) const {
    auto geometry = new osg::Geometry;
    
    const auto& vertices = mesh.vertices();
    const auto& indices = mesh.indices();
    
    // 顶点位置
    auto vertexArray = new osg::Vec3Array;
    vertexArray->reserve(vertices.positions.size());
    for (const auto& pos : vertices.positions) {
        vertexArray->push_back(osg::Vec3(pos[0], pos[1], pos[2]));
    }
    geometry->setVertexArray(vertexArray);
    
    // 法线
    if (!vertices.normals.empty()) {
        auto normalArray = new osg::Vec3Array;
        normalArray->reserve(vertices.normals.size());
        for (const auto& normal : vertices.normals) {
            normalArray->push_back(osg::Vec3(normal[0], normal[1], normal[2]));
        }
        geometry->setNormalArray(normalArray);
        geometry->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
    }
    
    // 颜色
    if (!vertices.colors.empty()) {
        auto colorArray = new osg::Vec4Array;
        colorArray->reserve(vertices.colors.size());
        for (const auto& color : vertices.colors) {
            colorArray->push_back(osg::Vec4(color[0] / 255.0f, color[1] / 255.0f, 
                                          color[2] / 255.0f, color[3] / 255.0f));
        }
        geometry->setColorArray(colorArray);
        geometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
    }
    
    // 纹理坐标
    if (!vertices.texCoords.empty()) {
        auto texCoordArray = new osg::Vec2Array;
        texCoordArray->reserve(vertices.texCoords.size());
        for (const auto& tc : vertices.texCoords) {
            texCoordArray->push_back(osg::Vec2(tc[0], tc[1]));
        }
        geometry->setTexCoordArray(0, texCoordArray);
    }
    
    // 索引
    auto drawElements = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
    drawElements->reserve(indices.size());
    for (const auto& index : indices) {
        drawElements->push_back(index);
    }
    geometry->addPrimitiveSet(drawElements);
    
    return geometry;
}

osg::ref_ptr<osg::LOD> StandardOsgExporter::createLodNode(const core::LodNode& lodNode) const {
    auto lodGroup = new osg::LOD;
    
    return std::visit([&](const auto& node) -> osg::ref_ptr<osg::LOD> {
        // 创建当前级别的几何体
        if (!node.mesh.empty()) {
            auto geometry = meshToGeometry(node.mesh);
            auto geode = new osg::Geode;
            geode->addDrawable(geometry);
            
            // 设置 LOD 距离
            float minRange = static_cast<float>(node.geometricError);
            float maxRange = minRange * 2.0f;
            
            lodGroup->addChild(geode, minRange, maxRange);
        }
        
        // 添加子节点
        for (const auto& child : node.children) {
            if (child) {
                auto childLod = createLodNode(*child);
                lodGroup->addChild(childLod);
            }
        }
        
        return lodGroup;
    }, lodNode);
}

void StandardOsgExporter::optimizeNode(osg::Node* node) const {
    if (!node) return;
    
    osgUtil::Optimizer optimizer;
    
    // 基本优化
    optimizer.optimize(node, 
        osgUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS |
        osgUtil::Optimizer::REMOVE_REDUNDANT_NODES |
        osgUtil::Optimizer::COMBINE_ADJACENT_LODS |
        osgUtil::Optimizer::SHARE_DUPLICATE_STATE);
    
    if (config_.mergeGeometry) {
        optimizer.optimize(node, osgUtil::Optimizer::MERGE_GEOMETRY);
    }
}

std::string StandardOsgExporter::generateFileName(const core::LodNode& node, const std::string& extension) const {
    return std::visit([&](const auto& lodNode) -> std::string {
        return "lod_level_" + std::to_string(lodNode.lodLevel) + "." + extension;
    }, node);
}

// HierarchicalOsgExporter 实现
std::expected<void, OsgError> HierarchicalOsgExporter::exportNode(const core::LodNode& node, const std::filesystem::path& outputPath) const {
    return standardExporter_.exportNode(node, outputPath);
}

std::expected<void, OsgError> HierarchicalOsgExporter::exportHierarchy(const core::LodNode& root, const std::filesystem::path& outputDir) const {
    createDirectoryStructure(outputDir, root);
    
    // 收集所有层级的节点
    std::map<int, std::vector<std::reference_wrapper<const core::LodNode>>> levelNodes;
    
    std::function<void(const core::LodNode&)> collectNodes = [&](const core::LodNode& node) {
        std::visit([&](const auto& lodNode) {
            levelNodes[lodNode.lodLevel].emplace_back(node);
            
            for (const auto& child : lodNode.children) {
                if (child) {
                    collectNodes(*child);
                }
            }
        }, node);
    };
    
    collectNodes(root);
    
    // 分层级导出
    for (const auto& [level, nodes] : levelNodes) {
        auto result = exportLevel(nodes, level, outputDir);
        if (!result) {
            return result;
        }
    }
    
    return {};
}

std::expected<void, OsgError> HierarchicalOsgExporter::exportSingleFile(const core::LodNode& root, const std::filesystem::path& outputFile) const {
    return standardExporter_.exportSingleFile(root, outputFile);
}

void HierarchicalOsgExporter::createDirectoryStructure(const std::filesystem::path& baseDir, const core::LodNode& root) const {
    std::filesystem::create_directories(baseDir);
    
    std::visit([&](const auto& node) {
        int maxLevel = node.lodLevel;
        
        std::function<void(const auto&)> findMaxLevel = [&](const auto& lodNode) {
            maxLevel = std::max(maxLevel, lodNode.lodLevel);
            for (const auto& child : lodNode.children) {
                if (child) {
                    findMaxLevel(*child);
                }
            }
        };
        
        findMaxLevel(node);
        
        // 为每个级别创建目录
        for (int i = 0; i <= maxLevel; ++i) {
            std::filesystem::create_directories(baseDir / ("level_" + std::to_string(i)));
        }
    }, root);
}

std::expected<void, OsgError> HierarchicalOsgExporter::exportLevel(const std::vector<std::reference_wrapper<const core::LodNode>>& nodes,
                                                                  int level, const std::filesystem::path& outputDir) const {
    auto levelDir = outputDir / ("level_" + std::to_string(level));
    
    int nodeIndex = 0;
    for (const auto& nodeRef : nodes) {
        auto fileName = "node_" + std::to_string(nodeIndex++) + ".osgb";
        auto filePath = levelDir / fileName;
        
        auto result = standardExporter_.exportNode(nodeRef.get(), filePath);
        if (!result) {
            return result;
        }
    }
    
    return {};
}

// 工厂函数实现
std::unique_ptr<IOsgExporter> createOsgExporter(const OsgExportConfig& config) {
    return std::make_unique<StandardOsgExporter>(config);
}

std::unique_ptr<HierarchicalOsgExporter> createHierarchicalOsgExporter(const OsgExportConfig& config) {
    return std::make_unique<HierarchicalOsgExporter>(config);
}

// 辅助函数实现
bool isOsgAvailable() noexcept {
    // 简单检查：尝试创建一个基本的 OSG 对象
    try {
        osg::Node* testNode = new osg::Node;
        testNode->unref();
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> getSupportedOsgFormats() noexcept {
    return {"osgb", "osgt", "osg", "ive", "3ds", "obj"};
}

} // namespace lod::io 