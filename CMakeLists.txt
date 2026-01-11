cmake_minimum_required(VERSION 3.10)
project(P2ProViewer)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

# Try pkg-config for libusb and ffmpeg
find_package(PkgConfig REQUIRED)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBUSB libusb-1.0)
    pkg_check_modules(SDL2_TTF SDL2_ttf)
    if(NOT APPLE)
        pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libswscale libavutil)
    endif()
endif()

if(NOT LIBUSB_FOUND)
    find_library(LIBUSB_LIBRARY NAMES usb-1.0)
    find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES libusb-1.0)
else()
    set(LIBUSB_LIBRARY ${LIBUSB_LIBRARIES})
    set(LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIRS})
endif()

if (NOT APPLE)
    include_directories(${FFMPEG_INCLUDE_DIRS})
    link_directories(${FFMPEG_LIBRARY_DIRS})
endif()

include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIR})
link_directories(${LIBUSB_LIBRARY_DIRS} ${SDL2_TTF_LIBRARY_DIRS})

if (APPLE)
    add_executable(P2ProViewer
            src/main.cpp
            src/P2Pro.cpp
            src/CameraWindow.cpp
            src/MacOSAdapter.cpp
            src/AVFoundationVideoSource.mm
            src/VideoRecorder_mac.mm
            src/ColorConversion.cpp
    )
else ()
    add_executable(P2ProViewer
            src/main.cpp
            src/P2Pro.cpp
            src/CameraWindow.cpp
            src/VideoRecorder_ffmpeg.cpp
            src/LinuxAdapter.cpp
            src/V4L2VideoSource.cpp
            src/ColorConversion.cpp
    )
endif ()

if (APPLE)
    target_link_libraries(P2ProViewer ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES} ${LIBUSB_LIBRARIES})
else()
    target_link_libraries(P2ProViewer ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES} ${LIBUSB_LIBRARIES} ${FFMPEG_LIBRARIES})
endif()

if(APPLE)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    find_library(AVFOUNDATION_FRAMEWORK AVFoundation)
    find_library(COREMEDIA_FRAMEWORK CoreMedia)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    target_link_libraries(P2ProViewer 
        ${IOKIT_FRAMEWORK} 
        ${COREFOUNDATION_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${AVFOUNDATION_FRAMEWORK}
        ${COREMEDIA_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
    )
    set_source_files_properties(src/AVFoundationVideoSource.mm src/VideoRecorder_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()
