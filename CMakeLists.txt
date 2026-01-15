cmake_minimum_required(VERSION 3.10)
project(P2ProViewer)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

# Try pkg-config for libusb and ffmpeg
find_package(PkgConfig REQUIRED)
if(PKG_CONFIG_FOUND)
    if(NOT APPLE)
        pkg_check_modules(LIBUSB libusb-1.0)
        pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libswscale libavutil)
    endif()
    pkg_check_modules(SDL2_TTF SDL2_ttf)
endif()

if(NOT APPLE)
    if(NOT LIBUSB_FOUND)
        find_library(LIBUSB_LIBRARIES NAMES usb-1.0)
        find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES libusb-1.0)
    else()
        set(LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIRS})
    endif()
endif()

if (NOT APPLE)
    include_directories(${FFMPEG_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIR})
    link_directories(${FFMPEG_LIBRARY_DIRS} ${LIBUSB_LIBRARY_DIRS})
endif()

include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
link_directories(${SDL2_TTF_LIBRARY_DIRS})

if (APPLE)
    add_executable(P2ProViewer
            src/main.cpp
            src/P2Pro.cpp
            src/CameraWindow.cpp
            src/MacOSAdapter.cpp
            src/AVFoundationVideoSource.mm
            src/VideoRecorder_mac.mm
            src/ColorConversion.cpp
            src/Scaler.cpp
            Resources/P2ProViewer.icns
    )
    set_target_properties(P2ProViewer PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_ICON_FILE P2ProViewer.icns
    )
    set_source_files_properties(Resources/P2ProViewer.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
else ()
    add_executable(P2ProViewer
            src/main.cpp
            src/P2Pro.cpp
            src/CameraWindow.cpp
            src/VideoRecorder_ffmpeg.cpp
            src/LinuxAdapter.cpp
            src/V4L2VideoSource.cpp
            src/ColorConversion.cpp
            src/Scaler.cpp
    )
endif ()

if (APPLE)
    # Prefer static linking for SDL2 and SDL2_ttf on macOS to make it more portable
    # We force the search for static libraries for these specific components
    set(ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})

    # Use pkg-config to find static versions and their dependencies if possible
    pkg_check_modules(SDL2_STATIC_PKG QUIET SDL2 STATIC IMPORTED_TARGET)
    pkg_check_modules(SDL2_TTF_STATIC_PKG QUIET SDL2_ttf STATIC IMPORTED_TARGET)

    if(TARGET PkgConfig::SDL2_TTF_STATIC_PKG)
        target_link_libraries(P2ProViewer PkgConfig::SDL2_TTF_STATIC_PKG)
    elseif(TARGET SDL2_ttf::SDL2_ttf-static)
        target_link_libraries(P2ProViewer SDL2_ttf::SDL2_ttf-static)
    else()
        find_library(SDL2_TTF_STATIC NAMES libSDL2_ttf.a)
        if(SDL2_TTF_STATIC)
            target_link_libraries(P2ProViewer ${SDL2_TTF_STATIC})
        else()
            target_link_libraries(P2ProViewer ${SDL2_TTF_LIBRARIES})
        endif()
    endif()

    if(NOT TARGET PkgConfig::SDL2_TTF_STATIC_PKG)
        if(TARGET PkgConfig::SDL2_STATIC_PKG)
            target_link_libraries(P2ProViewer PkgConfig::SDL2_STATIC_PKG)
        elseif(TARGET SDL2::SDL2-static)
            target_link_libraries(P2ProViewer SDL2::SDL2-static)
        else()
            find_library(SDL2_STATIC NAMES libSDL2.a)
            if(SDL2_STATIC)
                target_link_libraries(P2ProViewer ${SDL2_STATIC})
            else()
                target_link_libraries(P2ProViewer ${SDL2_LIBRARIES})
            endif()
        endif()
    endif()
    
    # Link against required frameworks for SDL2 static
    find_library(METAL_FRAMEWORK Metal)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(CARBON_FRAMEWORK Carbon)
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
    find_library(FORCEFEEDBACK_FRAMEWORK ForceFeedback)
    find_library(GAMECONTROLLER_FRAMEWORK GameController)
    target_link_libraries(P2ProViewer 
        ${METAL_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${CARBON_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOUNIT_FRAMEWORK}
        ${FORCEFEEDBACK_FRAMEWORK}
        ${GAMECONTROLLER_FRAMEWORK}
    )
    
    # Restore original suffixes
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
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

# CPack configuration
set(CPACK_PACKAGE_NAME "P2ProViewer")
set(CPACK_PACKAGE_VENDOR "P2Pro")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Viewer for P2 Pro thermal camera")
set(CPACK_PACKAGE_VERSION "1.0.0") # Will be overridden by semantic-release if needed, but good to have a default
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "P2Pro Maintainer")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0, libsdl2-ttf-2.0-0, libusb-1.0-0, libavcodec58 | libavcodec59 | libavcodec60, libavformat58 | libavformat59 | libavformat60, libswscale5 | libswscale6 | libswscale7, libavutil56 | libavutil57 | libavutil58")
set(CPACK_GENERATOR "DEB")

install(TARGETS P2ProViewer DESTINATION bin)
if(NOT APPLE)
    install(FILES 60-p2pro.rules DESTINATION lib/udev/rules.d)
    install(FILES Resources/icon.png DESTINATION share/pixmaps RENAME p2proviewer.png)
endif()

include(CPack)
