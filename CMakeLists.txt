cmake_minimum_required(VERSION 3.10)
project(P2ProViewer)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)
find_package(OpenCV REQUIRED)

# Try pkg-config again now that it might be installed or libusb is there
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBUSB libusb-1.0)
endif()

if(NOT LIBUSB_FOUND)
    find_library(LIBUSB_LIBRARY NAMES usb-1.0)
    find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES libusb-1.0)
else()
    set(LIBUSB_LIBRARY ${LIBUSB_LIBRARIES})
    set(LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIRS})
endif()

include_directories(${SDL2_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS})
link_directories(${LIBUSB_LIBRARY_DIRS})

if (APPLE)
    add_executable(P2ProViewer
            src/main.cpp
            src/P2Pro.cpp
            src/CameraWindow.cpp
            src/MacOSAdapter.cpp
            src/AVFoundationVideoSource.mm
            src/VideoRecorder.cpp
    )
else ()
    add_executable(P2ProViewer
            src/main.cpp
            src/P2Pro.cpp
            src/CameraWindow.cpp
            src/VideoRecorder.cpp
            src/LinuxAdapter.cpp
    )
endif ()

target_link_libraries(P2ProViewer ${SDL2_LIBRARIES} ${LIBUSB_LIBRARIES} ${OpenCV_LIBS})

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
    set_source_files_properties(src/AVFoundationVideoSource.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()
