#!/bin/bash

echo "LOD Generator 示例运行脚本"
echo

# 检查是否存在构建的可执行文件
if [ ! -f "build/release/src/lodgen" ]; then
    echo "错误：找不到 lodgen 可执行文件，请先构建项目"
    echo "运行命令：cmake --preset linux-release && cmake --build build/release"
    exit 1
fi

# 创建输出目录
mkdir -p output

# 运行 LOD 生成器
echo "正在运行 LOD 生成器..."
build/release/src/lodgen \
    --input examples/sample_input.txt \
    --output output \
    --format 3dtiles \
    --max-triangles 10000 \
    --max-levels 6 \
    --verbose \
    --log-file output/lodgen.log

if [ $? -eq 0 ]; then
    echo
    echo "成功完成 LOD 生成！"
    echo "输出文件位于 output 目录"
else
    echo
    echo "LOD 生成失败，错误代码：$?"
    echo "请查看日志文件：output/lodgen.log"
fi 