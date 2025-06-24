# LOD 生成器设计文档

## 目标
1. 输入多份 PLY 网格。
2. 依据地理坐标切分并按 LOD 分层。
3. 导出两种格式：OSGB（OpenSceneGraph）与 3D Tiles（b3dm/glb）。
4. 使用 vcpkg 统一包管理。
5. 代码风格强调函数式编程与设计模式相结合。

---

## 一、技术栈与依赖
| 分类 | 说明 |
| ---- | ---- |
| **语言/标准** | C++20（ranges、concepts 提升函数式表达力） |
| **构建系统** | CMake ≥ 3.26（Presets + vcpkg toolchain） |
| **包管理** | vcpkg（推荐 triplet: `x64-windows` / `x64-linux`） |
| **主要库** | openscenegraph、meshoptimizer、draco、gdal、proj、nlohmann-json、fmt、spdlog、Catch2/GTest、tinygltf（可选），并行库（TBB/OpenMP） |

---

## 二、目录结构
```text
LodGenerator/
├── CMakePresets.json          # 绑定 vcpkg triplet
├── vcpkg.json                 # 声明全部依赖
└── src/
    ├── core/                  # 纯算法，零 I/O
    │   ├── Mesh.hpp
    │   ├── Geometry.hpp
    │   └── LodAlgorithm.hpp
    ├── io/
    │   ├── PlyReader.hpp
    │   ├── OsbExporter.hpp
    │   └── TilesExporter.hpp
    ├── geo/
    │   ├── GeoBBox.hpp
    │   └── CRS.hpp
    ├── pipeline/              # 高层纯函数组合
    │   └── LodPipeline.hpp
    ├── app/                   # main()、CLI 解析
    │   └── main.cpp
    └── tests/
```

> 说明：`core`、`geo`、`pipeline` 保持无 I/O 耦合，IO 相关实现放在 `io` 层，通过接口注入。

---

## 三、核心数据结构（不可变 / 值语义）
```cpp
struct Mesh {
    Vertices v;
    Indices i;
    Attributes attr;
};

struct GeoBBox {
    double minLon, minLat, maxLon, maxLat; // WGS84
};

struct LodNode {
    GeoBBox region;
    std::vector<std::shared_ptr<LodNode>> children;
    Mesh mesh;              // 已简化后的网格
    int  lodLevel;
};
```
所有结构体尽量保持 POD 风格；修改即产生新实例，符合函数式理念。

---

## 四、主要算法流程
1. **读取**：PLY → `Mesh`。
2. **坐标转换**：若 PLY 自带投影 → 统一转 WGS84（`proj + gdal`）。
3. **切瓦片**：
   - 根据地理 `bbox` 构建四叉树 / 八叉树。
   - 叶节点阈值：角度 / 米 / 面片数。
4. **LOD 生成**：
   - 自底向上遍历，调用 `meshoptimizer::simplify()`。
   - 使用策略模式：`ILodStrategy`（面片数、屏幕误差、体素误差）。
5. **导出**：
   - OSGB：`osgDB::writeNodeFile`。
   - 3D Tiles：`tinygltf` 组装 glb → `draco` 压缩 → 封装 b3dm 容器。
6. **生成** `tileset.json`（层级、`geometricError` 等）。
7. **日志 & 计时**：`spdlog` + `std::chrono`。

---

## 五、设计模式映射
| 设计模式 | 场景 |
| -------- | ---- |
| Factory Method | `ExporterFactory::create(format)` 返回 `IExporter` |
| Strategy | LOD 简化策略 / 切瓦片策略 |
| Visitor  | 遍历 `LodNode` 执行不同导出器 |
| Builder  | `TilesetBuilder` 组装 `tileset.json` |
| Pipeline | `auto tiles = reader | transform | split | simplify | export;` 使用 ranges 管道表达 |

---

## 六、函数式编码准则
- 算法函数尽量 `constexpr` / `noexcept`，无隐藏状态。
- I/O、日志等副作用集中到 IO 层，通过依赖注入。
- 使用 `std::span` / `borrowed_range` 避免不必要拷贝，返回 `std::shared_ptr<const Mesh>`。
- 并行化：`std::transform_reduce`、`parallel_for`（TBB/OpenMP）。

---

## 七、构建与包管理示例
### 1. vcpkg.json
```json
{
  "name": "lod-generator",
  "version": "0.1.0",
  "dependencies": [
    "openscenegraph",
    "meshoptimizer",
    "draco",
    "proj4",
    "gdal",
    "nlohmann-json",
    "fmt",
    "spdlog",
    "catch2"
  ]
}
```

### 2. CMakePresets.json（片段）
```json
{
  "configurePresets": [
    {
      "name": "windows-release",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      }
    }
  ]
}
```

---

## 八、CLI 用法示例

### 1. 单文件模式（自动检测）
```bash
lodgen --input mesh.ply --output ./output --format 3dtiles
```

### 2. 地理坐标模式
```bash
lodgen --input geo_list.txt --output ./tiles --format 3dtiles --mode geo --max-triangles 10000
```
`geo_list.txt` 格式：
```
mesh1.ply 116.3974 39.9093 0.0 EPSG:4326
mesh2.ply 116.4074 39.9193 0.0 EPSG:4326
```

### 3. 纯几何模式（使用八叉树）
```bash
lodgen --input geometric_list.txt --output ./output --format 3dtiles --mode geometric --use-octree true
```
`geometric_list.txt` 格式：
```
mesh1.ply
mesh2.ply 10.0 0.0 0.0
mesh3.ply 0.0 10.0 0.0
```

### 4. 多文件自动合并
```bash
lodgen --input "mesh1.ply,mesh2.ply,mesh3.ply" --output ./output --mode auto
```

---

## 九、未来扩展
- 支持 Cesium quantized-mesh / slpk。
- 引入 GPU 加速（meshoptimizer SSE/AVX）。
- UI（ImGui）可视化预览 LOD 切片结果。
- 单元测试 & CI（GitHub Actions + vcpkg-cache）。

---

## 项目实施状态

### ✅ 已完成
- [x] 项目目录结构创建
- [x] vcpkg.json 依赖配置
- [x] CMake 构建系统设置
- [x] 核心数据结构设计（Mesh, GeoBBox, BoundingBox, LodNode）
- [x] 八叉树空间划分设计（OctreeNode, GeometricLodNode）
- [x] 双模式支持（地理坐标 + 纯几何）
- [x] I/O 接口设计（通用 PLY 读取器、OSG/3D Tiles 导出器）
- [x] 管道架构设计（函数式 + 设计模式）
- [x] 命令行界面设计（支持多种输入模式）
- [x] 单元测试框架搭建
- [x] 示例和文档（包含多种使用场景）

### 🚧 待实现
- [ ] 核心算法实现（网格简化、LOD 构建）
- [ ] I/O 模块具体实现
- [ ] 地理坐标转换实现
- [ ] 管道组件实现
- [ ] 单元测试用例完善

### 🎯 下一步行动
通过该设计文档可快速启动项目：
1. **环境准备**：安装 vcpkg 并设置环境变量
2. **依赖安装**：运行 `cmake --preset windows-release` 自动安装依赖
3. **核心实现**：先实现 `Mesh.cpp`、`GeoBBox.cpp` 等核心模块
4. **算法实现**：集成 meshoptimizer 实现网格简化
5. **I/O 实现**：实现 PLY 读取和格式导出
6. **测试验证**：运行单元测试确保功能正确
7. **集成测试**：使用示例数据验证完整流程

详细构建说明请参考 [BUILD.md](BUILD.md)。 