cmake_minimum_required(VERSION 3.10)

# Handle vcpkg toolchain BEFORE project() - setting it after project() has no effect
if(NOT CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    elseif(EXISTS "C:/vcpkg_root/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg_root/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    else()
        # Fall back to VS-bundled vcpkg (any edition/year)
        file(GLOB _vs_vcpkg
            "C:/Program Files/Microsoft Visual Studio/*/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
            "C:/Program Files (x86)/Microsoft Visual Studio/*/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
        )
        if(_vs_vcpkg)
            list(GET _vs_vcpkg 0 _vs_vcpkg_first)
            set(CMAKE_TOOLCHAIN_FILE "${_vs_vcpkg_first}" CACHE STRING "")
        endif()
    endif()
endif()

project(P2ProViewer)

set(CMAKE_CXX_STANDARD 17)

if(WIN32 AND NOT DEFINED USE_FFMPEG)
    set(USE_FFMPEG OFF CACHE BOOL "Use FFmpeg for video recording")
else()
    option(USE_FFMPEG "Use FFmpeg for video recording" ON)
endif()

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
    
set(SDL2_LIBRARIES SDL2::SDL2 SDL2::SDL2main)
set(SDL2_TTF_LIBRARIES SDL2_ttf::SDL2_ttf)

    # Try pkg-config for libusb and ffmpeg
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        if(NOT APPLE AND NOT WIN32)
            pkg_check_modules(LIBUSB libusb-1.0)
            if(USE_FFMPEG)
                pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libswscale libavutil)
            endif()
        endif()
    endif()

if(WIN32)
    # On Windows with vcpkg, libusb-1.0 and ffmpeg are found via find_package or find_library
    find_package(libusb-1.0 CONFIG)
    if(TARGET libusb-1.0::usb-1.0)
        set(LIBUSB_LIBRARIES libusb-1.0::usb-1.0)
    else()
        find_library(LIBUSB_LIBRARIES NAMES usb-1.0 libusb-1.0)
        find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES libusb-1.0)
        include_directories(${LIBUSB_INCLUDE_DIR})
    endif()

    if(USE_FFMPEG)
        find_package(FFMPEG COMPONENTS avcodec avformat swscale avutil REQUIRED)
        set(FFMPEG_LIBRARIES FFMPEG::avcodec FFMPEG::avformat FFMPEG::swscale FFMPEG::avutil)
        add_definitions(-DUSE_FFMPEG)
    endif()
    
elseif(NOT APPLE)
    if(NOT LIBUSB_FOUND)
        find_library(LIBUSB_LIBRARIES NAMES usb-1.0)
        find_path(LIBUSB_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES libusb-1.0)
    else()
        set(LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIRS})
    endif()
endif()

if (NOT APPLE AND NOT WIN32)
    include_directories(${FFMPEG_INCLUDE_DIRS} ${LIBUSB_INCLUDE_DIR})
    link_directories(${FFMPEG_LIBRARY_DIRS} ${LIBUSB_LIBRARY_DIRS})
endif()

if(WIN32)
    include_directories(${LIBUSB_INCLUDE_DIR})
endif()

include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
# link_directories is generally discouraged with modern CMake, 
# but we keep it if it was used for something specific.
# link_directories(${SDL2_TTF_LIBRARY_DIRS})

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
elseif(WIN32)
    set(WIN32_SOURCES
            src/main.cpp
            src/P2Pro.cpp
            src/CameraWindow.cpp
            src/WindowsAdapter.cpp
            src/MFVideoSource.cpp
            src/ColorConversion.cpp
            src/Scaler.cpp
    )
    if(USE_FFMPEG)
        list(APPEND WIN32_SOURCES src/VideoRecorder_ffmpeg.cpp)
    else()
        list(APPEND WIN32_SOURCES src/VideoRecorder_win.cpp)
    endif()

    add_executable(P2ProViewer ${WIN32_SOURCES})
    
    target_link_libraries(P2ProViewer 
        ${SDL2_LIBRARIES} 
        ${SDL2_TTF_LIBRARIES} 
        ${LIBUSB_LIBRARIES} 
        mfplat mf mfreadwrite mfuuid shlwapi ws2_32
    )
    if(USE_FFMPEG)
        target_link_libraries(P2ProViewer ${FFMPEG_LIBRARIES})
    endif()
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

if(APPLE)
    # Prefer static linking for SDL2 and SDL2_ttf on macOS to make it more portable
    # We force the search for static libraries for these specific components
    set(ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})

    # Use pkg-config to find static versions and their dependencies if possible
    # Note: We don't use IMPORTED_TARGET here to avoid issues with missing dependencies in the project environment
    pkg_check_modules(SDL2_STATIC_PKG QUIET SDL2 STATIC)
    pkg_check_modules(SDL2_TTF_STATIC_PKG QUIET SDL2_ttf STATIC)

    if(SDL2_TTF_STATIC_PKG_FOUND)
        target_link_libraries(P2ProViewer ${SDL2_TTF_STATIC_PKG_LIBRARIES})
        target_link_directories(P2ProViewer PRIVATE ${SDL2_TTF_STATIC_PKG_LIBRARY_DIRS})
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

    if(NOT SDL2_TTF_STATIC_PKG_FOUND)
        if(SDL2_STATIC_PKG_FOUND)
            target_link_libraries(P2ProViewer ${SDL2_STATIC_PKG_LIBRARIES})
            target_link_directories(P2ProViewer PRIVATE ${SDL2_STATIC_PKG_LIBRARY_DIRS})
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
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
    target_link_libraries(P2ProViewer
        ${METAL_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${CARBON_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOUNIT_FRAMEWORK}
        ${FORCEFEEDBACK_FRAMEWORK}
        ${GAMECONTROLLER_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        "-framework CoreHaptics"
        "-lobjc"
    )

    # Restore original suffixes
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})

    # Add common library search paths for macOS (Homebrew)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/opt/homebrew/lib -L/usr/local/lib")

    # Find and link freetype and harfbuzz if we're linking SDL2_ttf statically
    # These are needed because static libraries don't include their dependencies
    find_library(FREETYPE_LIB NAMES libfreetype.dylib freetype HINTS /opt/homebrew/lib /usr/local/lib)
    find_library(HARFBUZZ_LIB NAMES libharfbuzz.dylib harfbuzz HINTS /opt/homebrew/lib /usr/local/lib)
    
    if(FREETYPE_LIB)
        target_link_libraries(P2ProViewer "${FREETYPE_LIB}")
    else()
        target_link_libraries(P2ProViewer freetype)
    endif()
    
    if(HARFBUZZ_LIB)
        target_link_libraries(P2ProViewer "${HARFBUZZ_LIB}")
    else()
        target_link_libraries(P2ProViewer harfbuzz)
    endif()
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
