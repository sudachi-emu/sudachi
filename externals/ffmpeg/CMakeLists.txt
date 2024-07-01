# SPDX-FileCopyrightText: 2021 sudachi Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

if (NOT WIN32 AND NOT ANDROID)
    # Build FFmpeg from externals
    message(STATUS "Using FFmpeg from externals")

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|amd64)")
        # FFmpeg has source that requires one of nasm or yasm to assemble it.
        # REQUIRED throws an error if not found here during configuration rather than during compilation.
        find_program(ASSEMBLER NAMES nasm yasm)
        if ("${ASSEMBLER}" STREQUAL "ASSEMBLER-NOTFOUND")
            message(FATAL_ERROR "One of either `nasm` or `yasm` not found but is required.")
        endif()
    endif()

    find_program(AUTOCONF autoconf)
    if ("${AUTOCONF}" STREQUAL "AUTOCONF-NOTFOUND")
        message(FATAL_ERROR "Required program `autoconf` not found.")
    endif()

    set(FFmpeg_PREFIX ${PROJECT_SOURCE_DIR}/externals/ffmpeg/ffmpeg)
    set(FFmpeg_BUILD_DIR ${PROJECT_BINARY_DIR}/externals/ffmpeg-build)
    set(FFmpeg_MAKEFILE ${FFmpeg_BUILD_DIR}/Makefile)
    make_directory(${FFmpeg_BUILD_DIR})

    # Read version string from external
    file(READ ${FFmpeg_PREFIX}/RELEASE FFmpeg_VERSION)
    set(FFmpeg_FOUND NO)
    if (NOT FFmpeg_VERSION STREQUAL "")
        set(FFmpeg_FOUND YES)
    endif()

    unset(FFmpeg_LIBRARIES CACHE)
    foreach(COMPONENT ${FFmpeg_COMPONENTS})
        set(FFmpeg_${COMPONENT}_PREFIX "${FFmpeg_BUILD_DIR}/lib${COMPONENT}")
        set(FFmpeg_${COMPONENT}_LIB_NAME "lib${COMPONENT}.a")
        set(FFmpeg_${COMPONENT}_LIBRARY "${FFmpeg_${COMPONENT}_PREFIX}/${FFmpeg_${COMPONENT}_LIB_NAME}")

        set(FFmpeg_LIBRARIES
            ${FFmpeg_LIBRARIES}
            ${FFmpeg_${COMPONENT}_LIBRARY}
            CACHE PATH "Paths to FFmpeg libraries" FORCE)
    endforeach()

    find_package(PkgConfig REQUIRED)
    if (NOT ANDROID)
        pkg_check_modules(LIBVA libva)
        pkg_check_modules(CUDA cuda)
        pkg_check_modules(FFNVCODEC ffnvcodec)
        pkg_check_modules(VDPAU vdpau)
    endif()

    set(FFmpeg_HWACCEL_LIBRARIES)
    set(FFmpeg_HWACCEL_FLAGS)
    set(FFmpeg_HWACCEL_INCLUDE_DIRS)
    set(FFmpeg_HWACCEL_LDFLAGS)

    if(LIBVA_FOUND)
        pkg_check_modules(LIBDRM libdrm REQUIRED)
        find_package(X11 REQUIRED)
        pkg_check_modules(LIBVA-DRM libva-drm REQUIRED)
        pkg_check_modules(LIBVA-X11 libva-x11 REQUIRED)
        list(APPEND FFmpeg_HWACCEL_LIBRARIES
            ${LIBDRM_LIBRARIES}
            ${X11_LIBRARIES}
            ${LIBVA-DRM_LIBRARIES}
            ${LIBVA-X11_LIBRARIES}
            ${LIBVA_LIBRARIES})
        set(FFmpeg_HWACCEL_FLAGS
            --enable-hwaccel=h264_vaapi
            --enable-hwaccel=vp8_vaapi
            --enable-hwaccel=vp9_vaapi
            --enable-libdrm)
        list(APPEND FFmpeg_HWACCEL_INCLUDE_DIRS
            ${LIBDRM_INCLUDE_DIRS}
            ${X11_INCLUDE_DIRS}
            ${LIBVA-DRM_INCLUDE_DIRS}
            ${LIBVA-X11_INCLUDE_DIRS}
            ${LIBVA_INCLUDE_DIRS}
        )
        message(STATUS "VA-API found")
    else()
        set(FFmpeg_HWACCEL_FLAGS --disable-vaapi)
    endif()

    if (FFNVCODEC_FOUND)
        list(APPEND FFmpeg_HWACCEL_FLAGS
            --enable-cuvid
            --enable-ffnvcodec
            --enable-nvdec
            --enable-hwaccel=h264_nvdec
            --enable-hwaccel=vp8_nvdec
            --enable-hwaccel=vp9_nvdec
        )
        list(APPEND FFmpeg_HWACCEL_LIBRARIES ${FFNVCODEC_LIBRARIES})
        list(APPEND FFmpeg_HWACCEL_INCLUDE_DIRS ${FFNVCODEC_INCLUDE_DIRS})
        list(APPEND FFmpeg_HWACCEL_LDFLAGS ${FFNVCODEC_LDFLAGS})
        message(STATUS "ffnvcodec libraries version ${FFNVCODEC_VERSION} found")
        # ffnvenc could load CUDA libraries at the runtime using dlopen/dlsym or LoadLibrary/GetProcAddress
        # here we handle the hard-linking senario where CUDA is linked during compilation
        if (CUDA_FOUND)
            # This line causes build error if CUDA_INCLUDE_DIRS is anything but a single non-empty value
            #list(APPEND FFmpeg_HWACCEL_FLAGS --extra-cflags=-I${CUDA_INCLUDE_DIRS})
            list(APPEND FFmpeg_HWACCEL_LIBRARIES ${CUDA_LIBRARIES})
            list(APPEND FFmpeg_HWACCEL_INCLUDE_DIRS ${CUDA_INCLUDE_DIRS})
            list(APPEND FFmpeg_HWACCEL_LDFLAGS ${CUDA_LDFLAGS})
            message(STATUS "CUDA libraries found, hard-linking will be performed")
        endif(CUDA_FOUND)
    endif()

    if (VDPAU_FOUND)
        list(APPEND FFmpeg_HWACCEL_FLAGS
            --enable-vdpau
            --enable-hwaccel=h264_vdpau
            --enable-hwaccel=vp9_vdpau
        )
        list(APPEND FFmpeg_HWACCEL_LIBRARIES ${VDPAU_LIBRARIES})
        list(APPEND FFmpeg_HWACCEL_INCLUDE_DIRS ${VDPAU_INCLUDE_DIRS})
        list(APPEND FFmpeg_HWACCEL_LDFLAGS ${VDPAU_LDFLAGS})
        message(STATUS "vdpau libraries version ${VDPAU_VERSION} found")
    else()
        list(APPEND FFmpeg_HWACCEL_FLAGS --disable-vdpau)
    endif()

    find_program(BASH_PROGRAM bash REQUIRED)

    set(FFmpeg_CROSS_COMPILE_FLAGS "")
    if (ANDROID)
        string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}" FFmpeg_HOST_SYSTEM_NAME)
        set(TOOLCHAIN "${ANDROID_NDK}/toolchains/llvm/prebuilt/${FFmpeg_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}")
        set(SYSROOT "${TOOLCHAIN}/sysroot")
        set(FFmpeg_CPU "armv8-a")
        list(APPEND FFmpeg_CROSS_COMPILE_FLAGS
            --arch=arm64
            #--cpu=${FFmpeg_CPU}
            --enable-cross-compile
            --cross-prefix=${TOOLCHAIN}/bin/aarch64-linux-android-
            --sysroot=${SYSROOT}
            --target-os=android
            --extra-ldflags="--ld-path=${TOOLCHAIN}/bin/ld.lld"
            --extra-ldflags="-nostdlib"
        )
    endif()

    # `configure` parameters builds only exactly what sudachi needs from FFmpeg
    # `--disable-vdpau` is needed to avoid linking issues
    set(FFmpeg_CC ${CMAKE_C_COMPILER_LAUNCHER}  ${CMAKE_C_COMPILER})
    set(FFmpeg_CXX ${CMAKE_CXX_COMPILER_LAUNCHER}  ${CMAKE_CXX_COMPILER})
    add_custom_command(
        OUTPUT
            ${FFmpeg_MAKEFILE}
        COMMAND
            ${BASH_PROGRAM} ${FFmpeg_PREFIX}/configure
                --disable-avdevice
                --disable-avformat
                --disable-doc
                --disable-everything
                --disable-ffmpeg
                --disable-ffprobe
                --disable-network
                --disable-postproc
                --disable-swresample
                --enable-decoder=h264
                --enable-decoder=vp8
                --enable-decoder=vp9
                --enable-filter=yadif,scale
                --cc="${FFmpeg_CC}"
                --cxx="${FFmpeg_CXX}"
                ${FFmpeg_HWACCEL_FLAGS}
                ${FFmpeg_CROSS_COMPILE_FLAGS}
        WORKING_DIRECTORY
            ${FFmpeg_BUILD_DIR}
    )
    unset(FFmpeg_CC)
    unset(FFmpeg_CXX)
    unset(FFmpeg_HWACCEL_FLAGS)
    unset(FFmpeg_CROSS_COMPILE_FLAGS)

    # Workaround for Ubuntu 18.04's older version of make not being able to call make as a child
    # with context of the jobserver. Also helps ninja users.
    execute_process(
        COMMAND
            nproc
        OUTPUT_VARIABLE
            SYSTEM_THREADS)

    set(FFmpeg_BUILD_LIBRARIES ${FFmpeg_LIBRARIES})
    add_custom_command(
        OUTPUT
            ${FFmpeg_BUILD_LIBRARIES}
        COMMAND
            make -j${SYSTEM_THREADS}
        WORKING_DIRECTORY
            ${FFmpeg_BUILD_DIR}
    )

    set(FFmpeg_INCLUDE_DIR
        "${FFmpeg_PREFIX};${FFmpeg_BUILD_DIR};${FFmpeg_HWACCEL_INCLUDE_DIRS}"
        CACHE PATH "Path to FFmpeg headers" FORCE)

    set(FFmpeg_LDFLAGS
        "${FFmpeg_HWACCEL_LDFLAGS}"
        CACHE STRING "FFmpeg linker flags" FORCE)

    # ALL makes this custom target build every time
    # but it won't actually build if the DEPENDS parameter is up to date
    add_custom_target(ffmpeg-configure ALL DEPENDS ${FFmpeg_MAKEFILE})
    add_custom_target(ffmpeg-build ALL DEPENDS ${FFmpeg_BUILD_LIBRARIES} ffmpeg-configure)
    link_libraries(${FFmpeg_LIBVA_LIBRARIES})
    set(FFmpeg_LIBRARIES ${FFmpeg_BUILD_LIBRARIES} ${FFmpeg_HWACCEL_LIBRARIES}
        CACHE PATH "Paths to FFmpeg libraries" FORCE)
    unset(FFmpeg_BUILD_LIBRARIES)
    unset(FFmpeg_HWACCEL_FLAGS)
    unset(FFmpeg_HWACCEL_INCLUDE_DIRS)
    unset(FFmpeg_HWACCEL_LDFLAGS)
    unset(FFmpeg_HWACCEL_LIBRARIES)

    if (FFmpeg_FOUND)
        message(STATUS "Found FFmpeg version ${FFmpeg_VERSION}")
    else()
        message(FATAL_ERROR "FFmpeg not found")
    endif()
elseif(ANDROID)
    # Use sudachi FFmpeg binaries
    if (ARCHITECTURE_arm64)
        set(FFmpeg_EXT_NAME "ffmpeg-android-v5.1.LTS-aarch64")
    elseif (ARCHITECTURE_x86_64)
        set(FFmpeg_EXT_NAME "ffmpeg-android-v5.1.LTS-x86_64")
    else()
        message(FATAL_ERROR "Unsupported architecture for Android FFmpeg")
    endif()
    set(FFmpeg_PATH "${CMAKE_BINARY_DIR}/externals/${FFmpeg_EXT_NAME}")
    download_bundled_external("ffmpeg/" ${FFmpeg_EXT_NAME} "")
    set(FFmpeg_FOUND YES)
    set(FFmpeg_INCLUDE_DIR "${FFmpeg_PATH}/include" CACHE PATH "Path to FFmpeg headers" FORCE)
    set(FFmpeg_LIBRARY_DIR "${FFmpeg_PATH}/lib" CACHE PATH "Path to FFmpeg library directory" FORCE)
    set(FFmpeg_LDFLAGS "" CACHE STRING "FFmpeg linker flags" FORCE)
    set(FFmpeg_LIBRARIES
        ${FFmpeg_LIBRARY_DIR}/libavcodec.so
        ${FFmpeg_LIBRARY_DIR}/libavdevice.so
        ${FFmpeg_LIBRARY_DIR}/libavfilter.so
        ${FFmpeg_LIBRARY_DIR}/libavformat.so
        ${FFmpeg_LIBRARY_DIR}/libavutil.so
        ${FFmpeg_LIBRARY_DIR}/libswresample.so
        ${FFmpeg_LIBRARY_DIR}/libswscale.so
        ${FFmpeg_LIBRARY_DIR}/libvpx.a
        ${FFmpeg_LIBRARY_DIR}/libx264.a
        CACHE PATH "Paths to FFmpeg libraries" FORCE)
    # exported variables
    set(FFmpeg_PATH "${FFmpeg_PATH}" PARENT_SCOPE)
    set(FFmpeg_LDFLAGS "${FFmpeg_LDFLAGS}" PARENT_SCOPE)
    set(FFmpeg_LIBRARIES "${FFmpeg_LIBRARIES}" PARENT_SCOPE)
    set(FFmpeg_INCLUDE_DIR "${FFmpeg_INCLUDE_DIR}" PARENT_SCOPE)
elseif(WIN32)
    # Use sudachi FFmpeg binaries
    set(FFmpeg_EXT_NAME "ffmpeg-7.0")
    set(FFmpeg_PATH "${CMAKE_BINARY_DIR}/externals/${FFmpeg_EXT_NAME}")
    download_bundled_external("ffmpeg/" ${FFmpeg_EXT_NAME} "")
    set(FFmpeg_FOUND YES)
    set(FFmpeg_INCLUDE_DIR "${FFmpeg_PATH}/include" CACHE PATH "Path to FFmpeg headers" FORCE)
    set(FFmpeg_LIBRARY_DIR "${FFmpeg_PATH}/bin" CACHE PATH "Path to FFmpeg library directory" FORCE)
    set(FFmpeg_LDFLAGS "" CACHE STRING "FFmpeg linker flags" FORCE)
    set(FFmpeg_LIBRARIES
    ${FFmpeg_LIBRARY_DIR}/swscale.lib
    ${FFmpeg_LIBRARY_DIR}/avcodec.lib
    ${FFmpeg_LIBRARY_DIR}/avfilter.lib
    ${FFmpeg_LIBRARY_DIR}/avutil.lib
    CACHE PATH "Paths to FFmpeg libraries" FORCE)
    # exported variables
    set(FFmpeg_PATH "${FFmpeg_PATH}" PARENT_SCOPE)
    set(FFmpeg_LDFLAGS "${FFmpeg_LDFLAGS}" PARENT_SCOPE)
    set(FFmpeg_LIBRARIES "${FFmpeg_LIBRARIES}" PARENT_SCOPE)
    set(FFmpeg_INCLUDE_DIR "${FFmpeg_INCLUDE_DIR}" PARENT_SCOPE)
endif()

unset(FFmpeg_COMPONENTS)
