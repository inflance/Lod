@echo off
echo LOD Generator 单文件示例运行脚本
echo.

REM 检查是否存在构建的可执行文件
if not exist "build\release\src\lodgen.exe" (
    echo 错误：找不到 lodgen.exe，请先构建项目
    echo 运行命令：cmake --preset windows-release 然后 cmake --build build/release
    pause
    exit /b 1
)

REM 创建输出目录
if not exist output_single mkdir output_single

REM 运行 LOD 生成器（单文件模式）
echo 正在运行 LOD 生成器（单文件模式）...
build\release\src\lodgen.exe ^
    --input data\single_mesh.ply ^
    --output output_single ^
    --format 3dtiles ^
    --mode geometric ^
    --use-octree true ^
    --max-triangles 5000 ^
    --max-levels 6 ^
    --verbose ^
    --log-file output_single\lodgen.log

echo.
if %ERRORLEVEL% EQU 0 (
    echo 成功完成单文件 LOD 生成！
    echo 输出文件位于 output_single 目录
) else (
    echo LOD 生成失败，错误代码：%ERRORLEVEL%
    echo 请查看日志文件：output_single\lodgen.log
)

pause 