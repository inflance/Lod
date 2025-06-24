#pragma once

#include "../core/LodAlgorithm.hpp"
#include "../io/PlyReader.hpp"
#include "../io/OsgExporter.hpp"
#include "../io/TilesExporter.hpp"
#include <functional>
#include <expected>
#include <memory>

namespace lod::pipeline {

// 管道错误类型
enum class PipelineError {
    InputError,
    ProcessingError,
    OutputError,
    ConfigError
};

// 管道配置
struct PipelineConfig {
    // 输入配置（支持多种模式）
    io::InputConfig inputConfig;
    
    // LOD 配置
    core::LodConfig lodConfig;
    
    // 输出配置
    std::filesystem::path outputDirectory;
    std::vector<std::string> outputFormats;  // "osgb", "3dtiles"
    io::OsgExportConfig osgConfig;
    io::TilesExportConfig tilesConfig;
    
    // 处理配置
    bool enableParallelProcessing{true};
    size_t maxThreads{0};  // 0 = 自动检测
    bool enableProgressReporting{true};
    bool enableLogging{true};
    std::string logLevel{"info"};  // trace, debug, info, warn, error
    
    // 模式配置
    bool forceGeometricMode{false};  // 强制使用几何模式
    bool enableOctreeSubdivision{true};  // 启用八叉树细分
};

// 进度回调函数类型
using ProgressCallback = std::function<void(double progress, const std::string& message)>;
using LogCallback = std::function<void(const std::string& level, const std::string& message)>;

// 管道结果
struct PipelineResult {
    core::LodNode lodHierarchy;  // 使用变体类型支持两种模式
    core::LodStats stats;
    std::chrono::milliseconds processingTime;
    std::vector<std::filesystem::path> outputFiles;
    bool success{false};
    std::string errorMessage;
    core::LodMode lodMode;  // 实际使用的模式
};

// 函数式管道组件
namespace components {

// 输入阶段：读取 PLY 文件（通用）
[[nodiscard]] std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PipelineError>
loadInput(const io::InputConfig& inputConfig, 
          const ProgressCallback& progress = nullptr);

// 预处理阶段：坐标转换、网格清理（通用）
[[nodiscard]] std::expected<core::Mesh, PipelineError>
preprocessMesh(const core::Mesh& inputMesh, 
               const std::variant<geo::GeoBBox, core::BoundingBox>& bounds,
               const ProgressCallback& progress = nullptr);

// 核心处理阶段：构建 LOD 层次结构（通用）
[[nodiscard]] std::expected<core::LodNode, PipelineError>
buildLodHierarchy(const core::Mesh& mesh, 
                  const std::variant<geo::GeoBBox, core::BoundingBox>& bounds,
                  const core::LodConfig& config,
                  const ProgressCallback& progress = nullptr);

// 输出阶段：导出到各种格式（通用）
[[nodiscard]] std::expected<std::vector<std::filesystem::path>, PipelineError>
exportResults(const core::LodNode& lodRoot,
              const std::vector<std::string>& formats,
              const std::filesystem::path& outputDir,
              const io::OsgExportConfig& osgConfig,
              const io::TilesExportConfig& tilesConfig,
              const ProgressCallback& progress = nullptr);

} // namespace components

// 主管道类
class LodPipeline {
public:
    explicit LodPipeline(PipelineConfig config) 
        : config_(std::move(config)) {}
    
    // 执行完整管道
    [[nodiscard]] PipelineResult execute();
    
    // 执行管道（带回调）
    [[nodiscard]] PipelineResult execute(const ProgressCallback& progressCallback,
                                        const LogCallback& logCallback = nullptr);
    
    // 分步执行
    [[nodiscard]] std::expected<std::pair<core::Mesh, std::variant<geo::GeoBBox, core::BoundingBox>>, PipelineError> loadInput();
    [[nodiscard]] std::expected<core::Mesh, PipelineError> preprocessMesh(const core::Mesh& mesh, const std::variant<geo::GeoBBox, core::BoundingBox>& bounds);
    [[nodiscard]] std::expected<core::LodNode, PipelineError> buildLod(const core::Mesh& mesh, const std::variant<geo::GeoBBox, core::BoundingBox>& bounds);
    [[nodiscard]] std::expected<std::vector<std::filesystem::path>, PipelineError> exportResults(const core::LodNode& lodRoot);
    
    // 配置访问
    const PipelineConfig& config() const noexcept { return config_; }
    void updateConfig(PipelineConfig newConfig) { config_ = std::move(newConfig); }

private:
    PipelineConfig config_;
    
    // 内部状态
    mutable std::chrono::steady_clock::time_point startTime_;
    mutable double currentProgress_{0.0};
    
    // 辅助方法
    void updateProgress(double progress, const std::string& message, 
                       const ProgressCallback& callback) const;
    void log(const std::string& level, const std::string& message,
             const LogCallback& callback) const;
};

// 函数式管道构建器（使用 ranges 风格）
class PipelineBuilder {
public:
    PipelineBuilder() = default;
    
    // 链式配置
    PipelineBuilder& withInput(io::InputConfig inputConfig) {
        config_.inputConfig = std::move(inputConfig);
        return *this;
    }
    
    // 便利方法：单个文件
    PipelineBuilder& withSingleFile(const std::filesystem::path& filePath) {
        config_.inputConfig = filePath;
        return *this;
    }
    
    // 便利方法：多个文件（几何模式）
    PipelineBuilder& withMultipleFiles(std::vector<std::filesystem::path> filePaths) {
        config_.inputConfig = std::move(filePaths);
        return *this;
    }
    
    // 便利方法：地理模式文件
    PipelineBuilder& withGeoFiles(std::vector<io::PlyFileInfo> fileInfos) {
        config_.inputConfig = std::move(fileInfos);
        return *this;
    }
    
    PipelineBuilder& withLodConfig(core::LodConfig lodConfig) {
        config_.lodConfig = std::move(lodConfig);
        return *this;
    }
    
    PipelineBuilder& withOutput(std::filesystem::path outputDir, std::vector<std::string> formats) {
        config_.outputDirectory = std::move(outputDir);
        config_.outputFormats = std::move(formats);
        return *this;
    }
    
    PipelineBuilder& withParallelProcessing(bool enable, size_t maxThreads = 0) {
        config_.enableParallelProcessing = enable;
        config_.maxThreads = maxThreads;
        return *this;
    }
    
    PipelineBuilder& withLogging(bool enable, std::string level = "info") {
        config_.enableLogging = enable;
        config_.logLevel = std::move(level);
        return *this;
    }
    
    // 构建管道
    [[nodiscard]] LodPipeline build() {
        return LodPipeline{std::move(config_)};
    }
    
    // 直接执行（一次性使用）
    [[nodiscard]] PipelineResult execute() {
        return build().execute();
    }
    
    [[nodiscard]] PipelineResult execute(const ProgressCallback& progress, 
                                        const LogCallback& log = nullptr) {
        return build().execute(progress, log);
    }

private:
    PipelineConfig config_;
};

// 工厂函数和便利函数
[[nodiscard]] PipelineBuilder createPipeline();

// 便利函数：执行单文件 LOD 生成
[[nodiscard]] PipelineResult 
executeSingleFileLodGeneration(const std::filesystem::path& inputFile,
                               const std::filesystem::path& outputDir,
                               const std::vector<std::string>& formats = {"3dtiles"},
                               const ProgressCallback& progress = nullptr);

// 便利函数：执行多文件 LOD 生成
[[nodiscard]] PipelineResult 
executeMultiFileLodGeneration(const std::vector<std::filesystem::path>& inputFiles,
                              const std::filesystem::path& outputDir,
                              const std::vector<std::string>& formats = {"3dtiles"},
                              const ProgressCallback& progress = nullptr);

// 便利函数：执行地理 LOD 生成
[[nodiscard]] PipelineResult 
executeGeoLodGeneration(const std::vector<io::PlyFileInfo>& inputFiles,
                        const std::filesystem::path& outputDir,
                        const std::vector<std::string>& formats = {"3dtiles"},
                        const ProgressCallback& progress = nullptr);

// 验证配置
[[nodiscard]] std::expected<void, PipelineError> 
validateConfig(const PipelineConfig& config);

// 估算处理时间和资源需求
struct ResourceEstimate {
    std::chrono::seconds estimatedTime;
    size_t estimatedMemoryMB;
    size_t estimatedOutputSizeMB;
};

[[nodiscard]] ResourceEstimate 
estimateResources(const PipelineConfig& config);

} // namespace lod::pipeline 