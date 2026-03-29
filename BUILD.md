# NextView 构建指南

## 环境要求

- CMake 3.21+
- Qt 6.x（需要 Widgets, Multimedia, MultimediaWidgets, Concurrent 模块）
- C++17 兼容编译器（MSVC 2019+, GCC 10+, Clang 12+）
- FFmpeg（可选，用于视频预览生成）

## Windows 构建步骤

### 使用 Qt Creator（推荐）

1. 打开 Qt Creator
2. 文件 → 打开文件或项目 → 选择 `CMakeLists.txt`
3. 配置 Qt 6 Kit
4. 点击构建

### 命令行构建

```bash
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/msvc2019_64
cmake --build . --config Release
```

## macOS 构建步骤

```bash
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt6)
cmake --build . -j4
```

## FFmpeg 安装

### Windows
1. 从 https://github.com/BtbN/FFmpeg-Builds/releases 下载
2. 解压到 `C:/ffmpeg/`
3. 将 `C:/ffmpeg/bin` 添加到系统 PATH

### macOS
```bash
brew install ffmpeg
```

## 注意事项

- 视频预览功能需要 FFmpeg，没有 FFmpeg 时图片功能仍可正常使用
- 缩略图和预览视频缓存存储在系统应用数据目录
- Windows: `%APPDATA%/NextView/cache/`
- macOS: `~/Library/Application Support/NextView/cache/`
