cmake_minimum_required(VERSION 3.15)

project(MediaFoundationCamera LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加源文件
set(SOURCES
    main.cpp
    CameraCapture.cpp
)

# 添加可执行文件
add_executable(MediaFoundationCamera ${SOURCES})

# 链接必要的库
target_link_libraries(MediaFoundationCamera PRIVATE
    mfplat.lib
    mfreadwrite.lib
    mfuuid.lib
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
    evr.lib
)

# 添加预处理器定义
target_compile_definitions(MediaFoundationCamera PRIVATE
    NOMINMAX
    _USE_MATH_DEFINES
    WIN32_LEAN_AND_MEAN
)

# 设置包含目录
target_include_directories(MediaFoundationCamera PRIVATE
    ${CMAKE_SOURCE_DIR}
    $ENV{WindowsSdkDir}Include
    $ENV{WindowsSdkDir}Include/um
    $ENV{WindowsSdkDir}Include/shared
    $ENV{WindowsSdkDir}Include/ucrt
)

# 添加后期构建步骤以复制所需的 DLL 文件
add_custom_command(TARGET MediaFoundationCamera POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Windows/System32/d3dcompiler_47.dll"
        $<TARGET_FILE_DIR:MediaFoundationCamera>
)