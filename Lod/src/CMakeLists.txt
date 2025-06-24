# 核心库
add_library(lod_core
    core/Mesh.cpp
    core/Geometry.cpp
    core/LodAlgorithm.cpp
    geo/GeoBBox.cpp
    geo/CRS.cpp
)

target_include_directories(lod_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lod_core PUBLIC
    meshoptimizer::meshoptimizer
    TBB::tbb
)

# I/O 库
add_library(lod_io
    io/PlyReader.cpp
    io/OsgExporter.cpp
    io/TilesExporter.cpp
)

target_include_directories(lod_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${TINYGLTF_INCLUDE_DIRS}
)

target_link_libraries(lod_io PUBLIC
    lod_core
    ${OPENSCENEGRAPH_LIBRARIES}
    draco::draco
    GDAL::GDAL
    nlohmann_json::nlohmann_json
    fmt::fmt
    spdlog::spdlog
)

# 管道库
add_library(lod_pipeline
    pipeline/LodPipeline.cpp
)

target_include_directories(lod_pipeline PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lod_pipeline PUBLIC
    lod_core
    lod_io
    TBB::tbb
)

# 主应用程序
add_executable(lodgen
    app/main.cpp
)

target_link_libraries(lodgen PRIVATE
    lod_pipeline
    cxxopts::cxxopts
    spdlog::spdlog
    fmt::fmt
) 