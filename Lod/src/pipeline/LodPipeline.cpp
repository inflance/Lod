#include "LodPipeline.hpp"
#include <chrono>
#include <spdlog/spdlog.h>
#include <execution>

namespace lod::pipeline {

namespace components {

std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PipelineError>
loadInput(const io::InputConfig& inputConfig, const ProgressCallback& progress) {
    try {
        if (progress) {
            progress(0.1, "开始加载输入文件...");
        }
        
        auto reader = io::createUniversalPlyReader(inputConfig);
        auto result = reader->readInput();
        
        if (!result) {
            return std::unexpected(PipelineError::InputError);
        }
        
        if (progress) {
            progress(0.3, "输入文件加载完成");
        }
        
        return result.value();
    } catch (const std::exception&) {
        return std::unexpected(PipelineError::InputError);
    }
}

std::expected<core::Mesh, PipelineError>
preprocessMesh(const core::Mesh& inputMesh, 
               const std::variant<geo::GeoBBox, core::BoundingBox>& bounds,
               const ProgressCallback& progress) {
    try {
        if (progress) {
            progress(0.4, "开始预处理网格...");
        }
        
        // 当前简化实现：直接返回输入网格
        // 实际实现中应该包含：
        // - 坐标系转换
        // - 网格清理（去除重复顶点、修复法线等）
        // - 数据验证
        
        if (progress) {
            progress(0.5, "网格预处理完成");
        }
        
        return inputMesh;
    } catch (const std::exception&) {
        return std::unexpected(PipelineError::ProcessingError);
    }
}

std::expected<core::LodNode, PipelineError>
buildLodHierarchy(const core::Mesh& mesh, 
                  const std::variant<geo::GeoBBox, core::BoundingBox>& bounds,
                  const core::LodConfig& config,
                  const ProgressCallback& progress) {
    try {
        if (progress) {
            progress(0.6, "开始构建LOD层次结构...");
        }
        
        auto lodNode = core::buildLodHierarchy(mesh, bounds, config);
        
        if (progress) {
            progress(0.8, "LOD层次结构构建完成");
        }
        
        return lodNode;
    } catch (const std::exception&) {
        return std::unexpected(PipelineError::ProcessingError);
    }
}

std::expected<std::vector<std::filesystem::path>, PipelineError>
exportResults(const core::LodNode& lodRoot,
              const std::vector<std::string>& formats,
              const std::filesystem::path& outputDir,
              const io::OsgExportConfig& osgConfig,
              const io::TilesExportConfig& tilesConfig,
              const ProgressCallback& progress) {
    try {
        std::vector<std::filesystem::path> outputFiles;
        
        if (progress) {
            progress(0.85, "开始导出结果...");
        }
        
        // 确保输出目录存在
        std::filesystem::create_directories(outputDir);
        
        for (const auto& format : formats) {
            if (format == "osgb" || format == "osg") {
                auto exporter = io::createOsgExporter(osgConfig);
                auto outputPath = outputDir / ("result." + format);
                
                auto result = exporter->exportSingleFile(lodRoot, outputPath);
                if (result) {
                    outputFiles.push_back(outputPath);
                }
            } else if (format == "3dtiles") {
                auto exporter = io::createTilesExporter(io::TileFormat::B3DM, tilesConfig);
                auto tilesDir = outputDir / "3dtiles";
                
                auto result = exporter->exportTileset(lodRoot, tilesDir);
                if (result) {
                    outputFiles.push_back(tilesDir / "tileset.json");
                }
            }
        }
        
        if (progress) {
            progress(1.0, "导出完成");
        }
        
        return outputFiles;
    } catch (const std::exception&) {
        return std::unexpected(PipelineError::OutputError);
    }
}

} // namespace components

// LodPipeline 实现
PipelineResult LodPipeline::execute() {
    return execute(nullptr, nullptr);
}

PipelineResult LodPipeline::execute(const ProgressCallback& progressCallback,
                                   const LogCallback& logCallback) {
    PipelineResult result;
    result.success = false;
    startTime_ = std::chrono::steady_clock::now();
    
    try {
        log("info", "开始执行LOD生成管道", logCallback);
        
        // 步骤1: 加载输入
        updateProgress(0.1, "加载输入文件", progressCallback);
        auto inputResult = loadInput();
        if (!inputResult) {
            result.errorMessage = "输入加载失败";
            return result;
        }
        
        auto [mesh, bounds] = inputResult.value();
        result.lodMode = core::detectLodMode(bounds);
        
        // 步骤2: 预处理
        updateProgress(0.3, "预处理网格", progressCallback);
        auto preprocessResult = preprocessMesh(mesh, bounds);
        if (!preprocessResult) {
            result.errorMessage = "网格预处理失败";
            return result;
        }
        
        // 步骤3: 构建LOD
        updateProgress(0.5, "构建LOD层次结构", progressCallback);
        auto lodResult = buildLod(preprocessResult.value(), bounds);
        if (!lodResult) {
            result.errorMessage = "LOD构建失败";
            return result;
        }
        
        result.lodHierarchy = lodResult.value();
        
        // 计算统计信息
        result.stats = std::visit([](const auto& node) -> core::LodStats {
            if constexpr (std::is_same_v<std::decay_t<decltype(node)>, std::shared_ptr<core::GeoLodNode>>) {
                return core::computeGeoLodStats(*node);
            } else {
                return core::computeGeometricLodStats(*node);
            }
        }, result.lodHierarchy);
        
        // 步骤4: 导出结果
        updateProgress(0.8, "导出结果", progressCallback);
        auto exportResult = exportResults(result.lodHierarchy);
        if (!exportResult) {
            result.errorMessage = "结果导出失败";
            return result;
        }
        
        result.outputFiles = exportResult.value();
        result.success = true;
        
        updateProgress(1.0, "处理完成", progressCallback);
        
        auto endTime = std::chrono::steady_clock::now();
        result.processingTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime_);
        
        log("info", "LOD生成管道执行成功，耗时: " + 
            std::to_string(result.processingTime.count()) + "ms", logCallback);
        
    } catch (const std::exception& e) {
        result.errorMessage = std::string("执行异常: ") + e.what();
        log("error", result.errorMessage, logCallback);
    }
    
    return result;
}

std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PipelineError> 
LodPipeline::loadInput() {
    return components::loadInput(config_.inputConfig);
}

std::expected<core::Mesh, PipelineError> 
LodPipeline::preprocessMesh(const core::Mesh& mesh, const std::variant<geo::GeoBBox, core::BoundingBox>& bounds) {
    return components::preprocessMesh(mesh, bounds);
}

std::expected<core::LodNode, PipelineError> 
LodPipeline::buildLod(const core::Mesh& mesh, const std::variant<geo::GeoBBox, core::BoundingBox>& bounds) {
    return components::buildLodHierarchy(mesh, bounds, config_.lodConfig);
}

std::expected<std::vector<std::filesystem::path>, PipelineError> 
LodPipeline::exportResults(const core::LodNode& lodRoot) {
    return components::exportResults(lodRoot, config_.outputFormats, config_.outputDirectory,
                                   config_.osgConfig, config_.tilesConfig);
}

void LodPipeline::updateProgress(double progress, const std::string& message, 
                                const ProgressCallback& callback) const {
    currentProgress_ = progress;
    if (callback) {
        callback(progress, message);
    }
}

void LodPipeline::log(const std::string& level, const std::string& message,
                     const LogCallback& callback) const {
    if (config_.enableLogging) {
        if (callback) {
            callback(level, message);
        } else {
            // 使用默认日志记录
            if (level == "error") {
                spdlog::error(message);
            } else if (level == "warn") {
                spdlog::warn(message);
            } else if (level == "info") {
                spdlog::info(message);
            } else if (level == "debug") {
                spdlog::debug(message);
            } else {
                spdlog::trace(message);
            }
        }
    }
}

// 工厂函数和便利函数实现
PipelineBuilder createPipeline() {
    return PipelineBuilder{};
}

PipelineResult executeSingleFileLodGeneration(const std::filesystem::path& inputFile,
                                             const std::filesystem::path& outputDir,
                                             const std::vector<std::string>& formats,
                                             const ProgressCallback& progress) {
    return createPipeline()
        .withSingleFile(inputFile)
        .withOutput(outputDir, formats)
        .execute(progress);
}

PipelineResult executeMultiFileLodGeneration(const std::vector<std::filesystem::path>& inputFiles,
                                            const std::filesystem::path& outputDir,
                                            const std::vector<std::string>& formats,
                                            const ProgressCallback& progress) {
    return createPipeline()
        .withMultipleFiles(inputFiles)
        .withOutput(outputDir, formats)
        .execute(progress);
}

PipelineResult executeGeoLodGeneration(const std::vector<io::PlyFileInfo>& inputFiles,
                                      const std::filesystem::path& outputDir,
                                      const std::vector<std::string>& formats,
                                      const ProgressCallback& progress) {
    return createPipeline()
        .withGeoFiles(inputFiles)
        .withOutput(outputDir, formats)
        .execute(progress);
}

std::expected<void, PipelineError> validateConfig(const PipelineConfig& config) {
    // 验证输入配置
    bool validInput = std::visit([](const auto& inputConfig) -> bool {
        using T = std::decay_t<decltype(inputConfig)>;
        
        if constexpr (std::is_same_v<T, std::filesystem::path>) {
            return std::filesystem::exists(inputConfig);
        } else if constexpr (std::is_same_v<T, std::vector<std::filesystem::path>>) {
            return !inputConfig.empty() && 
                   std::all_of(inputConfig.begin(), inputConfig.end(),
                              [](const auto& path) { return std::filesystem::exists(path); });
        } else if constexpr (std::is_same_v<T, std::vector<io::PlyFileInfo>>) {
            return !inputConfig.empty() &&
                   std::all_of(inputConfig.begin(), inputConfig.end(),
                              [](const auto& info) { return std::filesystem::exists(info.filePath); });
        } else if constexpr (std::is_same_v<T, std::vector<io::SimplePlyFileInfo>>) {
            return !inputConfig.empty() &&
                   std::all_of(inputConfig.begin(), inputConfig.end(),
                              [](const auto& info) { return std::filesystem::exists(info.filePath); });
        }
        return false;
    }, config.inputConfig);
    
    if (!validInput) {
        return std::unexpected(PipelineError::ConfigError);
    }
    
    // 验证输出配置
    if (config.outputFormats.empty()) {
        return std::unexpected(PipelineError::ConfigError);
    }
    
    // 验证LOD配置
    if (!config.lodConfig.strategy) {
        return std::unexpected(PipelineError::ConfigError);
    }
    
    return {};
}

ResourceEstimate estimateResources(const PipelineConfig& config) {
    ResourceEstimate estimate;
    
    // 简化的资源估算
    size_t totalFiles = std::visit([](const auto& inputConfig) -> size_t {
        using T = std::decay_t<decltype(inputConfig)>;
        
        if constexpr (std::is_same_v<T, std::filesystem::path>) {
            return 1;
        } else if constexpr (std::is_same_v<T, std::vector<std::filesystem::path>>) {
            return inputConfig.size();
        } else if constexpr (std::is_same_v<T, std::vector<io::PlyFileInfo>>) {
            return inputConfig.size();
        } else if constexpr (std::is_same_v<T, std::vector<io::SimplePlyFileInfo>>) {
            return inputConfig.size();
        }
        return 0;
    }, config.inputConfig);
    
    // 基于文件数量的粗略估算
    estimate.estimatedTime = std::chrono::seconds(totalFiles * 10); // 每个文件大约10秒
    estimate.estimatedMemoryMB = totalFiles * 100; // 每个文件大约100MB内存
    estimate.estimatedOutputSizeMB = totalFiles * 50; // 每个文件大约50MB输出
    
    // 根据LOD配置调整
    estimate.estimatedTime = std::chrono::seconds(
        static_cast<long long>(estimate.estimatedTime.count() * config.lodConfig.maxLodLevels * 0.5));
    
    return estimate;
}

} // namespace lod::pipeline 