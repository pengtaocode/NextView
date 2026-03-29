cmake_minimum_required(VERSION 3.21)
project(NextView VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Multimedia
    MultimediaWidgets
    Concurrent
)

# 源文件列表
set(SOURCES
    src/main.cpp
    src/Models/ProjectManager.cpp
    src/Workers/ScanWorker.cpp
    src/Workers/CacheWorker.cpp
    src/Utils/CacheManager.cpp
    src/Utils/FFmpegHelper.cpp
    src/UI/GlassWidget.cpp
    src/UI/GlassNavBar.cpp
    src/UI/MainWindow.cpp
    src/UI/ProjectListPage.cpp
    src/UI/ProjectCard.cpp
    src/UI/BrowsePage.cpp
    src/UI/WaterfallWidget.cpp
    src/UI/MediaItemWidget.cpp
    src/UI/SettingsPanel.cpp
    src/UI/ImageViewer.cpp
    src/UI/VideoPlayerDialog.cpp
    src/UI/SplashOverlay.cpp
    src/UI/SearchBar.cpp
)

# 头文件列表
set(HEADERS
    src/Models/Project.h
    src/Models/MediaFile.h
    src/Models/ProjectManager.h
    src/Workers/ScanWorker.h
    src/Workers/CacheWorker.h
    src/Utils/CacheManager.h
    src/Utils/FFmpegHelper.h
    src/UI/GlassWidget.h
    src/UI/GlassNavBar.h
    src/UI/MarqueeLabel.h
    src/UI/MainWindow.h
    src/UI/ProjectListPage.h
    src/UI/ProjectCard.h
    src/UI/BrowsePage.h
    src/UI/WaterfallWidget.h
    src/UI/MediaItemWidget.h
    src/UI/SettingsPanel.h
    src/UI/ImageViewer.h
    src/UI/VideoPlayerDialog.h
    src/UI/ConfirmDialog.h
    src/UI/SplashOverlay.h
    src/UI/SearchBar.h
    src/UI/LoadingOverlay.h
)

# 资源文件
set(RESOURCES
    resources/resources.qrc
)

# 创建可执行文件
qt_add_executable(NextView
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# 包含目录
target_include_directories(NextView PRIVATE
    src
    src/Models
    src/Workers
    src/Utils
    src/UI
)

# 链接 Qt 库
target_link_libraries(NextView PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Concurrent
)

# Windows 平台设置
if(WIN32)
    set_target_properties(NextView PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    # 设置 Windows 应用图标（.rc 资源文件方式）
    set(APP_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
    file(WRITE "${APP_ICON_RC}" "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico\"\n")
    target_sources(NextView PRIVATE "${APP_ICON_RC}")
endif()

# macOS 平台设置
if(APPLE)
    set_target_properties(NextView PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "NextView"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    )
endif()
