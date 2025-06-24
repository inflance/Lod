cmake_minimum_required(VERSION 3.26)
project(LodGenerator VERSION 0.1.0 LANGUAGES CXX)

# C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(OpenSceneGraph REQUIRED COMPONENTS osgDB osgUtil osg OpenThreads)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(draco CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(GDAL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)
find_package(tinygltf CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# 包含目录
include_directories(src)

# 添加子目录
add_subdirectory(src)

# 启用测试
enable_testing()
add_subdirectory(tests) 