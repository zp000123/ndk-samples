# ============================================================
# 【CMake 最低版本要求】
# ============================================================
# cmake_minimum_required 指定构建此项目所需的最低 CMake 版本
# 3.22.1 是 Android Studio 较新版本支持的 CMake 版本
# 版本过低会导致某些 CMake 特性无法使用
cmake_minimum_required(VERSION 3.22.1)

# ============================================================
# 【项目定义】
# ============================================================
# project() 定义项目名称和使用的编程语言
# "hello-jni" 是项目名称
# LANGUAGES CXX 表示使用 C++ 语言（会自动启用 C++ 编译器）
project("hello-jni" LANGUAGES CXX)

# ============================================================
# 【包含辅助模块】
# ============================================================
# include() 加载其他 CMake 脚本文件
# AppLibrary.cmake 位于项目根目录的 cmake/ 文件夹中
# 它定义了 add_app_library() 函数，封装了创建 Android 库的常见配置
# 包括：编译选项、符号版本控制、链接选项等
include(AppLibrary)

# ============================================================
# 【查找依赖包】
# ============================================================
# find_package() 查找外部库或包
# base 是项目内部定义的基础库（位于 base/ 目录）
# CONFIG 表示使用 <Package>Config.cmake 配置文件方式查找
# REQUIRED 表示这是必需依赖，找不到会报错停止构建
# 找到后，可以使用 base::base 目标链接到我们的库
find_package(base CONFIG REQUIRED)

# ============================================================
# 【创建 Native 库】
# ============================================================
# add_app_library() 是项目自定义的函数（在 AppLibrary.cmake 中定义）
# 它是 add_library() 的包装，添加了项目特定的构建设置
#
# 参数说明：
#   hello-jni - 库的名称，最终生成 libhello-jni.so
#   SHARED - 构建为动态共享库（.so 文件），Android 应用通常使用 SHARED
#            其他选项：STATIC（静态库 .a）、MODULE（插件模块）
#   hello-jni.cpp - 源文件列表，可以指定多个文件
#
# 【Android 库类型说明】
# - SHARED（推荐）：动态库，体积小，可被多个应用共享，运行时加载
# - STATIC：静态库，编译时链接到可执行文件，会增加 APK 体积
# - MODULE：特殊用途，很少使用
add_app_library(hello-jni SHARED
    hello-jni.cpp
)

# ============================================================
# 【链接库依赖】
# ============================================================
# target_link_libraries() 指定链接时需要的库
# 现代 CMake 推荐使用 target_xxx 命令，它们会传递依赖关系
#
# PRIVATE 关键字含义：
# - PRIVATE：仅当前目标使用，不传递给依赖本库的其他目标
# - PUBLIC：当前目标使用，也传递给依赖本库的其他目标
# - INTERFACE：仅传递给依赖本库的其他目标，当前目标自己不使用
#
# 【链接的库说明】
# 1. base::base - 项目内部基础库，提供日志、宏定义等通用功能
# 2. android - Android NDK 提供的核心库，包含 Android 特定的 API
# 3. log - Android 日志库，提供 __android_log_print 等日志函数
#
# 【NDK 常用系统库】
# - android: Android 核心 API（如 AAssetManager、ANativeWindow）
# - log: 日志输出（logcat）
# - EGL: OpenGL ES 渲染上下文
# - GLESv3: OpenGL ES 3.0 图形 API
# - OpenSLES: 音频 API（已废弃，推荐使用 Oboe）
# - camera2ndk: 相机 API
# - mediandk: 媒体编解码 API
target_link_libraries(hello-jni
    PRIVATE
    base::base
    android
    log
)
