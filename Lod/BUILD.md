# LOD Generator 构建指南

## 环境要求

### 系统要求
- Windows 10/11 或 Linux (Ubuntu 20.04+)
- CMake 3.26 或更高版本
- C++20 兼容的编译器（MSVC 2022, GCC 11+, Clang 14+）
- Git

### 依赖管理
本项目使用 vcpkg 进行依赖管理，所有第三方库将自动下载和构建。

## 快速开始

### 1. 克隆项目
```bash
git clone <repository-url>
cd LodGenerator
```

### 2. 安装 vcpkg（如果尚未安装）
```bash
# Windows
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Linux
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg integrate install
```

### 3. 设置环境变量
```bash
# Windows (PowerShell)
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

# Linux/macOS
export VCPKG_ROOT=/path/to/vcpkg
```

### 4. 配置和构建

#### Windows (Visual Studio)
```bash
# 配置（Debug）
cmake --preset windows-debug

# 构建
cmake --build build/debug

# 配置（Release）
cmake --preset windows-release
cmake --build build/release
```

#### Linux
```bash
# 配置（Debug）
cmake --preset linux-debug

# 构建
cmake --build build/debug

# 配置（Release）
cmake --preset linux-release
cmake --build build/release
```

## 依赖库详情

### 核心依赖
- **OpenSceneGraph**: 3D 图形和 OSGB 格式支持
- **meshoptimizer**: 网格优化和简化
- **draco**: 几何压缩（用于 3D Tiles）
- **GDAL**: 地理空间数据处理
- **proj**: 坐标系转换

### 辅助依赖
- **nlohmann-json**: JSON 处理（tileset.json）
- **fmt**: 字符串格式化
- **spdlog**: 日志系统
- **TBB**: 并行处理
- **tinygltf**: glTF/GLB 格式支持

### 开发依赖
- **Catch2**: 单元测试框架
- **cxxopts**: 命令行参数解析

## 构建选项

### CMake 选项
```bash
# 启用/禁用组件
-DENABLE_TESTS=ON/OFF          # 构建测试（默认：ON）
-DENABLE_EXAMPLES=ON/OFF       # 构建示例（默认：ON）
-DENABLE_DRACO=ON/OFF          # 启用 Draco 压缩（默认：ON）
-DENABLE_PARALLEL=ON/OFF       # 启用并行处理（默认：ON）

# 优化选项
-DCMAKE_BUILD_TYPE=Release     # 发布版本
-DCMAKE_BUILD_TYPE=Debug       # 调试版本
-DCMAKE_BUILD_TYPE=RelWithDebInfo  # 带调试信息的发布版本
```

### vcpkg Triplet
```bash
# Windows
-DVCPKG_TARGET_TRIPLET=x64-windows        # 动态链接
-DVCPKG_TARGET_TRIPLET=x64-windows-static # 静态链接

# Linux
-DVCPKG_TARGET_TRIPLET=x64-linux          # 动态链接
-DVCPKG_TARGET_TRIPLET=x64-linux-static   # 静态链接
```

## 测试

### 运行单元测试
```bash
# 构建后运行测试
cd build/release
ctest --verbose

# 或直接运行测试可执行文件
./tests/lod_tests
```

### 运行示例
```bash
# Windows
examples\run_example.bat

# Linux
chmod +x examples/run_example.sh
./examples/run_example.sh
```

## 故障排除

### 常见问题

1. **vcpkg 依赖安装失败**
   - 确保网络连接正常
   - 检查 `VCPKG_ROOT` 环境变量
   - 尝试清理 vcpkg 缓存：`vcpkg remove --outdated`

2. **编译错误**
   - 检查 C++20 支持
   - 确保 CMake 版本 ≥ 3.26
   - 查看具体错误信息并检查依赖版本

3. **链接错误**
   - 检查 triplet 设置是否一致
   - 确保所有依赖都使用相同的运行时库
   - Windows 上注意 MSVC 运行时版本

4. **运行时错误**
   - 检查 DLL 路径（Windows）
   - 确保所有依赖库都已正确安装
   - 查看日志文件获取详细错误信息

### 获取帮助
- 查看项目 Issues
- 检查依赖库的官方文档
- 使用 `--verbose` 选项获取详细输出

## 开发环境设置

### Visual Studio Code
推荐安装以下扩展：
- C/C++ Extension Pack
- CMake Tools
- vcpkg integration

### Visual Studio
- 确保安装 C++ CMake tools
- 启用 vcpkg 集成

### CLion
- 配置 CMake 工具链
- 设置 vcpkg 工具链文件路径 