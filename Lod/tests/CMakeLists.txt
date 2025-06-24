# 测试可执行文件
add_executable(lod_tests
    test_main.cpp
    test_mesh.cpp
    test_geobbox.cpp
    test_geometry.cpp
    test_lod_algorithm.cpp
    test_pipeline.cpp
)

target_link_libraries(lod_tests PRIVATE
    lod_core
    lod_io
    lod_pipeline
    Catch2::Catch2WithMain
)

# 注册测试
include(CTest)
include(Catch)
catch_discover_tests(lod_tests) 