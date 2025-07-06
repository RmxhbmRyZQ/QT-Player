cmake_minimum_required(VERSION 3.31)
project(Player)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 添加 FFmpeg 的头文件目录
include_directories("D:/AVDevelop/build64/ffmpeg/include")
include_directories("D:/AVDevelop/build64/include")
include_directories("${CMAKE_SOURCE_DIR}/ui")
include_directories("${CMAKE_SOURCE_DIR}/player/include")

# 添加 FFmpeg 的库文件目录
link_directories("D:/AVDevelop/build64/ffmpeg/lib")
link_directories("D:/AVDevelop/build64/lib")

# 设置 Qt 安装路径
set(CMAKE_PREFIX_PATH "D:/Qt/6.9.1/mingw_64")

# 查找 Qt6 所需模块：Core + Widgets
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets Multimedia)

# 可执行文件
add_executable(Player WIN32 main.cpp ui/PlayerWindow.cpp ui/PlayerWindow.ui resources/app.qrc resources/appicon.rc
        player/Player.cpp
        player/include/Player.h
        player/PacketDemux.cpp
        player/include/PacketDemux.h
        player/AudioDecoder.cpp
        player/include/AudioDecoder.h
        player/VideoDecoder.cpp
        player/include/VideoDecoder.h
        player/include/PlayerContext.h
        player/AVPacketQueue.cpp
        player/include/AVPacketQueue.h
        player/AVFrameQueue.cpp
        player/include/AVFrameQueue.h
        player/AudioFilter.cpp
        player/include/AudioFilter.h
        player/VideoFilter.cpp
        player/include/VideoFilter.h
        player/PlayerContext.cpp
        player/AudioBuffer.cpp
        player/include/AudioBuffer.h
        ui/VideoRenderWidget.cpp
        ui/VideoRenderWidget.h
        player/VideoRender.cpp
        player/include/VideoRender.h
        player/AudioRender.cpp
        player/include/AudioRender.h
        player/include/constant.h
        ui/ClickableSlider.cpp
        ui/ClickableSlider.h
        ui/ProgressSlider.cpp
        ui/ProgressSlider.h
        ui/ThumbnailPreview.cpp
        ui/ThumbnailPreview.h
        player/ThumbnailWorker.cpp
        player/include/ThumbnailWorker.h)

# 链接 Qt6 库
target_link_libraries(Player PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Multimedia
        avformat
        avcodec
        avutil
        swresample
        swscale
        avdevice
        avfilter
)

# 复制 DLL 和插件
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()

    # 拷贝平台插件（必要）
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()

    # 拷贝 Qt DLL（Core + Widgets）
    foreach(QT_LIB Core Widgets)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach()

    # 拷贝 Qt imageformats 插件（含 qjpeg）
    foreach(PLUGIN_NAME qjpeg qpng qico qgif qsvg)
        if (EXISTS "${QT_INSTALL_PATH}/plugins/imageformats/${PLUGIN_NAME}${DEBUG_SUFFIX}.dll")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/imageformats/")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy
                    "${QT_INSTALL_PATH}/plugins/imageformats/${PLUGIN_NAME}${DEBUG_SUFFIX}.dll"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/imageformats/")
        endif ()
    endforeach()
endif ()
