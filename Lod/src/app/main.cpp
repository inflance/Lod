#include "../pipeline/LodPipeline.hpp"
#include "../io/PlyReader.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

using namespace lod;

// 命令行选项结构
struct CommandLineOptions {
    std::string inputFile;
    std::string outputDir;
    std::vector<std::string> formats{"3dtiles"};
    std::string mode{"auto"};  // auto, geo, geometric
    std::string crs{"EPSG:4326"};
    size_t maxTriangles{50000};
    int maxLevels{8};
    double reductionRatio{0.5};
    bool useOctree{true};
    bool enableParallel{true};
    size_t maxThreads{0};
    bool verbose{false};
    bool quiet{false};
    std::string logFile;
    bool showProgress{true};
    bool dryRun{false};
};

// 解析命令行参数
std::expected<CommandLineOptions, std::string> parseCommandLine(int argc, char* argv[]) {
    try {
        cxxopts::Options options("lodgen", "LOD Generator for PLY meshes with geographic tiling");
        
        options.add_options()
            ("i,input", "Input PLY file or file list", cxxopts::value<std::string>())
            ("o,output", "Output directory", cxxopts::value<std::string>())
            ("f,format", "Output formats (osgb,3dtiles)", cxxopts::value<std::vector<std::string>>()->default_value("3dtiles"))
            ("mode", "LOD mode (auto,geo,geometric)", cxxopts::value<std::string>()->default_value("auto"))
            ("crs", "Coordinate reference system", cxxopts::value<std::string>()->default_value("EPSG:4326"))
            ("max-triangles", "Maximum triangles per tile", cxxopts::value<size_t>()->default_value("50000"))
            ("max-levels", "Maximum LOD levels", cxxopts::value<int>()->default_value("8"))
            ("reduction-ratio", "Triangle reduction ratio per level", cxxopts::value<double>()->default_value("0.5"))
            ("use-octree", "Use octree subdivision", cxxopts::value<bool>()->default_value("true"))
            ("parallel", "Enable parallel processing", cxxopts::value<bool>()->default_value("true"))
            ("max-threads", "Maximum threads (0=auto)", cxxopts::value<size_t>()->default_value("0"))
            ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
            ("q,quiet", "Quiet mode", cxxopts::value<bool>()->default_value("false"))
            ("log-file", "Log file path", cxxopts::value<std::string>())
            ("no-progress", "Disable progress bar", cxxopts::value<bool>()->default_value("false"))
            ("dry-run", "Dry run (validate only)", cxxopts::value<bool>()->default_value("false"))
            ("h,help", "Show help");
        
        auto result = options.parse(argc, argv);
        
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            std::exit(0);
        }
        
        CommandLineOptions opts;
        
        if (result.count("input")) {
            opts.inputFile = result["input"].as<std::string>();
        } else {
            return std::unexpected("Input file is required");
        }
        
        if (result.count("output")) {
            opts.outputDir = result["output"].as<std::string>();
        } else {
            return std::unexpected("Output directory is required");
        }
        
        opts.formats = result["format"].as<std::vector<std::string>>();
        opts.mode = result["mode"].as<std::string>();
        opts.crs = result["crs"].as<std::string>();
        opts.maxTriangles = result["max-triangles"].as<size_t>();
        opts.maxLevels = result["max-levels"].as<int>();
        opts.reductionRatio = result["reduction-ratio"].as<double>();
        opts.useOctree = result["use-octree"].as<bool>();
        opts.enableParallel = result["parallel"].as<bool>();
        opts.maxThreads = result["max-threads"].as<size_t>();
        opts.verbose = result["verbose"].as<bool>();
        opts.quiet = result["quiet"].as<bool>();
        opts.showProgress = !result["no-progress"].as<bool>();
        opts.dryRun = result["dry-run"].as<bool>();
        
        if (result.count("log-file")) {
            opts.logFile = result["log-file"].as<std::string>();
        }
        
        return opts;
        
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Command line parsing error: ") + e.what());
    }
}

// 设置日志系统
void setupLogging(const CommandLineOptions& opts) {
    std::vector<spdlog::sink_ptr> sinks;
    
    if (!opts.quiet) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(opts.verbose ? spdlog::level::debug : spdlog::level::info);
        sinks.push_back(console_sink);
    }
    
    if (!opts.logFile.empty()) {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(opts.logFile, true);
        file_sink->set_level(spdlog::level::trace);
        sinks.push_back(file_sink);
    }
    
    auto logger = std::make_shared<spdlog::logger>("lodgen", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    spdlog::set_default_logger(logger);
    
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
}

// 进度回调
void progressCallback(double progress, const std::string& message) {
    static auto lastUpdate = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    // 限制更新频率（每100ms）
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() < 100 
        && progress < 1.0) {
        return;
    }
    lastUpdate = now;
    
    // 简单的进度条
    const int barWidth = 50;
    int pos = static_cast<int>(barWidth * progress);
    
    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << static_cast<int>(progress * 100.0) << "% " << message;
    std::cout.flush();
    
    if (progress >= 1.0) {
        std::cout << std::endl;
    }
}

// 日志回调
void logCallback(const std::string& level, const std::string& message) {
    if (level == "trace") spdlog::trace(message);
    else if (level == "debug") spdlog::debug(message);
    else if (level == "info") spdlog::info(message);
    else if (level == "warn") spdlog::warn(message);
    else if (level == "error") spdlog::error(message);
    else spdlog::info(message);
}

// 构建管道配置
pipeline::PipelineConfig buildPipelineConfig(const CommandLineOptions& opts) {
    pipeline::PipelineConfig config;
    
    // 自动检测输入类型并创建配置
    auto inputConfigResult = io::autoDetectInputConfig(opts.inputFile);
    if (!inputConfigResult) {
        throw std::runtime_error("Failed to detect input configuration: " + opts.inputFile);
    }
    config.inputConfig = std::move(*inputConfigResult);
    
    // LOD 配置
    config.lodConfig.strategy = std::make_unique<core::TriangleCountStrategy>(
        opts.maxTriangles, opts.reductionRatio);
    config.lodConfig.maxLodLevels = opts.maxLevels;
    config.lodConfig.enableParallelProcessing = opts.enableParallel;
    config.lodConfig.useOctreeSubdivision = opts.useOctree;
    
    // 模式配置
    if (opts.mode == "geometric") {
        config.forceGeometricMode = true;
    } else if (opts.mode == "geo") {
        config.forceGeometricMode = false;
    }
    // auto 模式由管道自动检测
    
    config.enableOctreeSubdivision = opts.useOctree;
    
    // 输出配置
    config.outputDirectory = opts.outputDir;
    config.outputFormats = opts.formats;
    
    // 处理配置
    config.enableParallelProcessing = opts.enableParallel;
    config.maxThreads = opts.maxThreads;
    config.enableProgressReporting = opts.showProgress;
    config.enableLogging = true;
    config.logLevel = opts.verbose ? "debug" : "info";
    
    return config;
}

// 显示结果摘要
void showResultSummary(const pipeline::PipelineResult& result) {
    spdlog::info("=== LOD Generation Complete ===");
    spdlog::info("Success: {}", result.success ? "Yes" : "No");
    spdlog::info("LOD Mode: {}", result.lodMode == core::LodMode::Geographic ? "Geographic" : "Geometric");
    
    if (!result.success) {
        spdlog::error("Error: {}", result.errorMessage);
        return;
    }
    
    spdlog::info("Processing time: {:.2f} seconds", result.processingTime.count() / 1000.0);
    
    // 显示统计信息（根据模式）
    std::visit([](const auto& stats) {
        spdlog::info("Total nodes: {}", stats.totalNodes);
        spdlog::info("Leaf nodes: {}", stats.leafNodes);
        spdlog::info("Total triangles: {}", stats.totalTriangles);
        spdlog::info("Max depth: {}", stats.maxDepth);
        
        // 显示每层级的三角形数量
        if (!stats.trianglesPerLevel.empty()) {
            spdlog::info("Triangles per level:");
            for (size_t i = 0; i < stats.trianglesPerLevel.size(); ++i) {
                spdlog::info("  Level {}: {} triangles", i, stats.trianglesPerLevel[i]);
            }
        }
        
        // 显示边界信息
        if constexpr (std::is_same_v<std::decay_t<decltype(stats)>, core::GeoLodStats>) {
            spdlog::info("Geographic bounds: [{:.6f}, {:.6f}] to [{:.6f}, {:.6f}]", 
                        stats.totalRegion.minLon, stats.totalRegion.minLat,
                        stats.totalRegion.maxLon, stats.totalRegion.maxLat);
        } else {
            spdlog::info("Bounding box: [{:.3f}, {:.3f}, {:.3f}] to [{:.3f}, {:.3f}, {:.3f}]",
                        stats.totalBounds.min[0], stats.totalBounds.min[1], stats.totalBounds.min[2],
                        stats.totalBounds.max[0], stats.totalBounds.max[1], stats.totalBounds.max[2]);
        }
    }, result.stats);
    
    spdlog::info("Output files:");
    for (const auto& file : result.outputFiles) {
        spdlog::info("  - {}", file.string());
    }
}

int main(int argc, char* argv[]) {
    try {
        // 解析命令行
        auto optsResult = parseCommandLine(argc, argv);
        if (!optsResult) {
            std::cerr << "Error: " << optsResult.error() << std::endl;
            return 1;
        }
        const auto opts = *optsResult;
        
        // 设置日志
        setupLogging(opts);
        
        spdlog::info("LOD Generator v0.1.0");
        spdlog::info("Input: {}", opts.inputFile);
        spdlog::info("Output: {}", opts.outputDir);
        spdlog::info("Formats: {}", fmt::join(opts.formats, ", "));
        spdlog::info("Mode: {}", opts.mode);
        spdlog::info("Use Octree: {}", opts.useOctree ? "Yes" : "No");
        
        // 构建管道配置
        auto config = buildPipelineConfig(opts);
        
        // 验证配置
        auto validation = pipeline::validateConfig(config);
        if (!validation) {
            spdlog::error("Configuration validation failed");
            return 1;
        }
        
        // 资源估算
        auto estimate = pipeline::estimateResources(config);
        spdlog::info("Estimated processing time: {} seconds", estimate.estimatedTime.count());
        spdlog::info("Estimated memory usage: {} MB", estimate.estimatedMemoryMB);
        spdlog::info("Estimated output size: {} MB", estimate.estimatedOutputSizeMB);
        
        if (opts.dryRun) {
            spdlog::info("Dry run completed successfully");
            return 0;
        }
        
        // 执行管道
        spdlog::info("Starting LOD generation...");
        
        auto pipeline = pipeline::LodPipeline{std::move(config)};
        auto result = pipeline.execute(
            opts.showProgress ? progressCallback : pipeline::ProgressCallback{},
            logCallback
        );
        
        // 显示结果
        showResultSummary(result);
        
        return result.success ? 0 : 1;
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    } catch (...) {
        spdlog::error("Unknown fatal error occurred");
        return 1;
    }
} 