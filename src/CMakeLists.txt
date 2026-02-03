# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0097 NEW)

project(mediaxx_library VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IS_LINUX_D    0)
set(IS_WIN_D      0)
set(IS_MACOS_D    0)
set(IS_ANDROID_D  0)
set(IS_IOS_D      0)

if (WIN32)
  set(IS_WIN_D 1)
elseif (ANDROID)
  set(IS_ANDROID_D 1)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(IS_MACOS_D 1)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(IS_IOS_D 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(IS_LINUX_D 1)
endif ()

add_definitions(-DIS_LINUX_D=${IS_LINUX_D})
add_definitions(-DIS_WIN_D=${IS_WIN_D})
add_definitions(-DIS_MACOS_D=${IS_MACOS_D})
add_definitions(-DIS_ANDROID_D=${IS_ANDROID_D})
add_definitions(-DIS_IOS_D=${IS_IOS_D})

message("[mediaxx] Current Configure Type: ${CMAKE_CONFIGURATION_TYPES}")
message("[mediaxx] Current Build Type: ${CMAKE_BUILD_TYPE}")

set(STATIC_LINK_FFMPEG OFF CACHE STRING "是否编译时静态链接 ffmepg")
set(STATIC_LINK_LIBMPV OFF CACHE STRING "是否编译时静态链接 libmpv")
set(EXPORT_ALL_SYMBOL OFF CACHE STRING "导出所有符号")
set(EXPORT_FULL_SYMBOL OFF CACHE STRING "[windows only]导出ffmpeg.exe/ffprobe.exe需要的符号")

if (NOT DEFINED LUMENXX_BUILD_TYPE AND NOT STATIC_LINK_FFMPEG)
  # release 时不需要，由主程序提供静态链接ffmpeg的动态库
  message("[mediaxx] 跳过编译 libmediaxx ，期望由主程序提供预编译动态库")
  return()
endif()

message("[mediaxx] CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
if (MSVC)
		set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}    /utf-8 /MD /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE}  /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++23preview")
		set(CMAKE_C_FLAGS_PROFILE     "${CMAKE_C_FLAGS_PROFILE}    /utf-8 /MD /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS_PROFILE   "${CMAKE_CXX_FLAGS_PROFILE}  /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++23preview")
		set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}      /utf-8 /MD /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}    /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++23preview")

    # set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(_MEDIAXX_C_FLAGS   "-fvisibility=default -ffunction-sections -fdata-sections -Wall -fPIC -pipe -std=c17")
    set(_MEDIAXX_CXX_FLAGS "-fvisibility=default -ffunction-sections -fdata-sections -Wall -fPIC -pipe -std=c++23")
    set(LDFLAGS "${LDFLAGS} -Wl,--gc-sections")

    if (DEFINED CMAKE_CONFIGURATION_TYPES)
      set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}   ${_MEDIAXX_C_FLAGS}   ")
      set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} ${_MEDIAXX_CXX_FLAGS} -fno-exceptions -fno-rtti")
      set(CMAKE_C_FLAGS_PROFILE     "${CMAKE_C_FLAGS_PROFILE}   ${_MEDIAXX_C_FLAGS}   ")
      set(CMAKE_CXX_FLAGS_PROFILE   "${CMAKE_CXX_FLAGS_PROFILE} ${_MEDIAXX_CXX_FLAGS} -fno-exceptions -fno-rtti")
      set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}     ${_MEDIAXX_C_FLAGS}   -fno-omit-frame-pointer -fno-optimize-sibling-calls")
      set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}   ${_MEDIAXX_CXX_FLAGS} -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    else ()
      if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS}     ${_MEDIAXX_C_FLAGS}   -fno-omit-frame-pointer -fno-optimize-sibling-calls")
        set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS}   ${_MEDIAXX_CXX_FLAGS} -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-optimize-sibling-calls")
      else ()
        set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS}   ${_MEDIAXX_C_FLAGS}     ")
        set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} ${_MEDIAXX_CXX_FLAGS}   -fno-exceptions -fno-rtti")
      endif ()
    endif ()

    if (IS_ANDROID_D OR IS_LINUX_D) 
      # static link to ffmpeg.a
      # windows 不需要
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
    endif()
else ()
  message("[mediaxx] 不支持的编译器")
  return()
endif ()

set(EXE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/exec")
message("[mediaxx] CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message("[mediaxx] EXE_INSTALL_PREFIX: ${EXE_INSTALL_PREFIX}")
message("[mediaxx] CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

add_definitions(-D__STDC_CONSTANT_MACROS)
if (DEFINED LUMENXX_BUILD_TYPE)
  add_definitions(-DLUMENXX_BUILD_TYPE=${LUMENXX_BUILD_TYPE})
endif()

include(ExternalProject)

# simdjson
ExternalProject_Add(
  simdjson_repo
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/simdjson/"
  UPDATE_COMMAND ""
  CMAKE_ARGS 
      -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
      -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      "-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}"
      "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}" 
      "-DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH}"

      "-DCMAKE_ANDROID_ARCH_ABI=${CMAKE_ANDROID_ARCH_ABI}"
      "-DANDROID_PLATFORM=${ANDROID_PLATFORM}"
      "-DANDROID_ABI=${ANDROID_ABI}"
      "-DANDROID_NDK=${ANDROID_NDK}"
      "-DANDROID_STL=${ANDROID_STL}"
      "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
      "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"

      "-DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}"
      "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"

      "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
      "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
      "-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}"
      "-DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}"
      "-DCMAKE_C_FLAGS_PROFILE=${CMAKE_C_FLAGS_PROFILE}"
      "-DCMAKE_CXX_FLAGS_PROFILE=${CMAKE_CXX_FLAGS_PROFILE}"
      "-DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}"
      "-DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}"
      -DBUILD_SHARED_LIBS=OFF
      -DSIMDJSON_BUILD_STATIC_LIB=ON
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(simdjson_repo SOURCE_DIR)
set(simdjson_SOURCE_DIR "${SOURCE_DIR}")
ExternalProject_Get_Property(simdjson_repo BINARY_DIR)
set(simdjson_INSTALL_DIR "${BINARY_DIR}/")

# fmt
ExternalProject_Add(
  fmt_repo
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/"
  UPDATE_COMMAND ""
  CMAKE_ARGS 
      -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
      -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      "-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}"
      "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}" 
      "-DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH}"

      "-DCMAKE_ANDROID_ARCH_ABI=${CMAKE_ANDROID_ARCH_ABI}"
      "-DANDROID_PLATFORM=${ANDROID_PLATFORM}"
      "-DANDROID_ABI=${ANDROID_ABI}"
      "-DANDROID_NDK=${ANDROID_NDK}"
      "-DANDROID_STL=${ANDROID_STL}"
      "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
      "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"

      "-DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}"
      "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"

      "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
      "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
      "-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}"
      "-DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}"
      "-DCMAKE_C_FLAGS_PROFILE=${CMAKE_C_FLAGS_PROFILE}"
      "-DCMAKE_CXX_FLAGS_PROFILE=${CMAKE_CXX_FLAGS_PROFILE}"
      "-DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}"
      "-DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}"
      -DFMT_PEDANTIC=OFF
      -DFMT_INSTALL=OFF
      -DFMT_WERROR=OFF
      -DBUILD_SHARED_LIBS=OFF
      -DFMT_DOC=OFF
      -DFMT_TEST=OFF
      -DFMT_CUDA_TEST=OFF
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(fmt_repo SOURCE_DIR)
set(fmt_SOURCE_DIR "${SOURCE_DIR}")
ExternalProject_Get_Property(fmt_repo BINARY_DIR)
set(fmt_INSTALL_DIR "${BINARY_DIR}/")

if (DEFINED LUMENXX_BUILD_TYPE)
  # uchardet
  set(UCHARDET_BINARY_DIR ${CMAKE_BINARY_DIR}/uchardet_repo-build)
  set(UCHARDET_INSTALL_DIR ${CMAKE_BINARY_DIR}/uchardet_repo-install)
  ExternalProject_Add(
    uchardet_repo
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/uchardet/"
    BINARY_DIR ${UCHARDET_BINARY_DIR}
    INSTALL_DIR ${UCHARDET_INSTALL_DIR}
    CMAKE_ARGS 
        "-DCMAKE_INSTALL_PREFIX=${UCHARDET_INSTALL_DIR}"
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        "-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}"
        "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}" 
        "-DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH}"

        "-DCMAKE_ANDROID_ARCH_ABI=${CMAKE_ANDROID_ARCH_ABI}"
        "-DANDROID_PLATFORM=${ANDROID_PLATFORM}"
        "-DANDROID_ABI=${ANDROID_ABI}"
        "-DANDROID_NDK=${ANDROID_NDK}"
        "-DANDROID_STL=${ANDROID_STL}"
        "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"

        "-DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"

        "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
        "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
        "-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}"
        "-DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}"
        "-DCMAKE_C_FLAGS_PROFILE=${CMAKE_C_FLAGS_PROFILE}"
        "-DCMAKE_CXX_FLAGS_PROFILE=${CMAKE_CXX_FLAGS_PROFILE}"
        "-DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}"
        "-DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}"
        -DBUILD_BINARY=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DCHECK_SSE2=ON
    UPDATE_COMMAND ""
    TEST_COMMAND ""
    LOG_INSTALL ON
  )
  ExternalProject_Get_Property(uchardet_repo SOURCE_DIR)
  set(uchardet_SOURCE_DIR "${SOURCE_DIR}")

  # iconv
  set(LIBICONV_VERSION 1.18)
  set(LIBICONV_URL "https://ftp.gnu.org/gnu/libiconv/libiconv-${LIBICONV_VERSION}.tar.gz")
  set(LIBICONV_PREFIX ${CMAKE_BINARY_DIR}/libiconv-src)
  set(LIBICONV_SOURCE_DIR ${LIBICONV_PREFIX}/src/iconv_repo)
  set(LIBICONV_BINARY_DIR ${CMAKE_BINARY_DIR}/libiconv-build)
  set(LIBICONV_INSTALL_DIR ${CMAKE_BINARY_DIR}/libiconv-install)
  ExternalProject_Add(
    iconv_repo
    URL ${LIBICONV_URL}
    PREFIX ${LIBICONV_PREFIX} 
    SOURCE_DIR ${LIBICONV_SOURCE_DIR}
    BINARY_DIR ${LIBICONV_BINARY_DIR} 
    INSTALL_DIR ${LIBICONV_INSTALL_DIR} 
    CONFIGURE_COMMAND 
        cd ${LIBICONV_SOURCE_DIR} && ./configure
        --prefix=${LIBICONV_INSTALL_DIR} 
        --host=${CMAKE_C_COMPILER_TARGET}
        --enable-static=yes
        --enable-shared=no
        --enable-pic
        --disable-rpath
        --disable-nls
        --disable-libiconv-close
        CC=${CMAKE_C_COMPILER}
        CXX=${CMAKE_CXX_COMPILER}
        AR=${CMAKE_AR}
        RANLIB=${CMAKE_RANLIB}
        "CFLAGS=${CMAKE_C_FLAGS} -DANDROID_PLATFORM=${ANDROID_PLATFORM} -DANDROID_ABI=${ANDROID_ABI} -DANDROID_NDK=${ANDROID_NDK} -DANDROID_STL=${ANDROID_STL}"
    BUILD_COMMAND 
      cd ${LIBICONV_SOURCE_DIR} && make -j${CMAKE_BUILD_PARALLEL_LEVEL}
    INSTALL_COMMAND 
      cd ${LIBICONV_SOURCE_DIR} && make install
    UPDATE_COMMAND ""
    TEST_COMMAND ""
  )
endif ()

# - .vscode/c_cpp_properties.json 默认取 android
# - 两种模式，动态链接 ffmpeg 和 静态链接 ffmpeg
#   - 静态链接时需要各种 ffmpeg 的依赖库，因此只在发布 release 时上 docker 内编译，最终合并 ffmpeg 到 mediaxx 内整合为一个动态库
set(ffmpeg_mpv_CFLAGS "")
set(ffmpeg_mpv_INCLUDE_DIR ".")
set(ffmpeg_mpv_BINARY_DIR ".")
set(ffmpeg_mpv_LIBRARIES 
  avcodec
  avdevice
  avfilter
  avformat
  avutil
  swresample
  swscale
)
if (STATIC_LINK_LIBMPV) 
  set (ffmpeg_mpv_LIBRARIES "${ffmpeg_mpv_LIBRARIES} mpv") 
endif ()
set(third_party_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")
set(vulkan_INCLUDE_DIR ".")
# - 编译 libmediaxx 动态库时，如果静态链接 ffmpeg，下面的文件指定了最终导出的符号
# - 应当包含：
#     - `mediaxx.h` 导出的符号
#     - `安卓JNI接口`
#     - `libmpv` 可以引用的，部分静态链接库符号也由嵌入 libmediaxx 的 ffmpeg 导出，被libmpv引用，可大幅减少`libmpv` 的体积
# - 限制符号导出可以大幅减少 `libmediaxx`，不建议包含标准库的导出符号，这可能导致内存管理混乱
# - 更新 `ffmpeg`、`mediaxx`、`libmpv` 时，需要检查更新符号表
set(mediaxx_EXPORT_DEF "")
set(mediaxx_EXPORT_IDS "")
set(mediaxx_EXPORT_KEEP_SYMBOL "")

if (STATIC_LINK_FFMPEG)
  # 静态链接，合并 ffmpeg 
  find_package(PkgConfig REQUIRED)

  set(PKG_CONFIG_FLAGS "--static")

  pkg_check_modules(AVCODEC REQUIRED libavcodec) 
  pkg_check_modules(AVDEVICE REQUIRED libavdevice)
  pkg_check_modules(AVFILTER REQUIRED libavfilter)
  pkg_check_modules(AVFORMAT REQUIRED libavformat)
  pkg_check_modules(AVUTIL REQUIRED libavutil)
  pkg_check_modules(SWRESAMPLE REQUIRED libswresample)
  pkg_check_modules(SWSCALE REQUIRED libswscale)

  if (STATIC_LINK_LIBMPV)
    pkg_check_modules(LIBMPV REQUIRED mpv)
  endif()

  set(ffmpeg_mpv_CFLAGS 
    ${LIBMPV_CFLAGS}
    ${AVCODEC_CFLAGS}
    ${AVDEVICE_CFLAGS}
    ${AVFILTER_CFLAGS}
    ${AVFORMAT_CFLAGS}
    ${AVUTIL_CFLAGS}
    ${SWRESAMPLE_CFLAGS}
    ${SWSCALE_CFLAGS}
  )
  set(ffmpeg_mpv_BINARY_DIR 
    ${LIBMPV_LIBRARY_DIRS}
    ${AVCODEC_LIBRARY_DIRS}
    ${AVDEVICE_LIBRARY_DIRS}
    ${AVFILTER_LIBRARY_DIRS}
    ${AVFORMAT_LIBRARY_DIRS}
    ${AVUTIL_LIBRARY_DIRS}
    ${SWRESAMPLE_LIBRARY_DIRS}
    ${SWSCALE_LIBRARY_DIRS}
  )
  set(ffmpeg_mpv_LIBRARIES 
    ${LIBMPV_LIBRARIES}
    avcodec
    avdevice
    avfilter
    avformat
    avutil
    swresample
    swscale
    ${AVCODEC_LIBRARIES}
    ${AVDEVICE_LIBRARIES}
    ${AVFILTER_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    m
  )
  set(ffmpeg_mpv_INCLUDE_DIR 
    ${LIBMPV_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVDEVICE_INCLUDE_DIRS}
    ${AVFILTER_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
  )
endif ()

if (IS_WIN_D)
  if (NOT STATIC_LINK_FFMPEG)
    set(ffmpeg_mpv_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/libs-win/")
    set(ffmpeg_mpv_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/include-win/")
    set(vulkan_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan-header/include/")
  else ()
    if (EXPORT_FULL_SYMBOL)
      set(mediaxx_EXPORT_DEF "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-win-full.def")
      set(mediaxx_EXPORT_KEEP_SYMBOL "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-win-full-symbols.txt")
    else ()
      set(mediaxx_EXPORT_DEF "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-win.def")
      set(mediaxx_EXPORT_KEEP_SYMBOL "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-win-symbols.txt")
    endif ()
  endif ()
elseif (IS_ANDROID_D)
  message("[mediaxx] build for android | ANDROID_ABI: ${ANDROID_ABI}")

  if (NOT STATIC_LINK_FFMPEG)
    set(ffmpeg_mpv_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/libs-android/${ANDROID_ABI}/")
    set(ffmpeg_mpv_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/include-android/")
  else ()
    set(mediaxx_EXPORT_IDS "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-android.lds")
    set(mediaxx_EXPORT_KEEP_SYMBOL "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-android-symbols.txt")
  endif ()
elseif (IS_LINUX_D)
  if (NOT STATIC_LINK_FFMPEG)
    set(ffmpeg_mpv_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/libs-linux/")
    set(ffmpeg_mpv_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/include-linux/")
  else ()
    set(mediaxx_EXPORT_IDS "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-linux.lds")
    set(mediaxx_EXPORT_KEEP_SYMBOL "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-linux-symbols.txt")
  endif ()
elseif (IS_IOS_D)
  set(mediaxx_EXPORT_IDS "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-apple.exp")
  set(mediaxx_EXPORT_KEEP_SYMBOL "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-apple-symbols.txt")
elseif (IS_MACOS_D)
  if (NOT STATIC_LINK_FFMPEG)
    set(ffmpeg_mpv_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/libs-macos/")
    set(ffmpeg_mpv_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/include-macos/")
  else ()
    set(mediaxx_EXPORT_IDS "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-apple.exp")
    set(mediaxx_EXPORT_KEEP_SYMBOL "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-apple-symbols.txt")
  endif()
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/)
include_directories(${simdjson_SOURCE_DIR}/include/)
include_directories(${fmt_SOURCE_DIR}/include/)
include_directories(${UCHARDET_INSTALL_DIR}/include/)
include_directories(${LIBICONV_INSTALL_DIR}/include/)
include_directories(${vulkan_INCLUDE_DIR})
include_directories(${third_party_DIR})
include_directories(${ffmpeg_mpv_INCLUDE_DIR})

aux_source_directory("./lib/util" DIR_UTIL)
aux_source_directory("./lib/analyse" DIR_ANALYSE)
aux_source_directory("./impl" DIR_IMPL)
aux_source_directory("./impl/android" DIR_IMPL_ANDROID)

add_library(mediaxx SHARED
  ${DIR_IMPL}
  ${DIR_IMPL_ANDROID}
  ${DIR_UTIL}
  ${DIR_ANALYSE}

  ${mediaxx_EXPORT_DEF}
)

add_dependencies(mediaxx simdjson_repo)
add_dependencies(mediaxx fmt_repo)
if (DEFINED LUMENXX_BUILD_TYPE)
  add_dependencies(mediaxx uchardet_repo)
  add_dependencies(mediaxx iconv_repo)
  target_link_directories(mediaxx PUBLIC 
    ${UCHARDET_INSTALL_DIR}/lib
    ${LIBICONV_INSTALL_DIR}/lib
  )
endif ()

target_link_directories(mediaxx PUBLIC 
  ${ffmpeg_mpv_BINARY_DIR}
  ${simdjson_INSTALL_DIR}
  ${fmt_INSTALL_DIR}
)

target_link_libraries(mediaxx PUBLIC
  ${ffmpeg_mpv_LIBRARIES}
  simdjson
  uchardet
  iconv
)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  # 仅单配置构建、debug 时使用
  target_link_libraries(mediaxx PUBLIC
    fmtd
  )
else ()
  target_link_libraries(mediaxx PUBLIC
    fmt
  )
endif()

target_compile_options(mediaxx PRIVATE 
    ${ffmpeg_mpv_CFLAGS}
)

# 统一输出名称 libmediaxx
# - 不指定时，部分系统，如win可能编译输出缺少前缀 lib
set_target_properties(mediaxx PROPERTIES
  PUBLIC_HEADER mediaxx.h
  PREFIX "lib"
  OUTPUT_NAME "mediaxx"
)

target_compile_definitions(mediaxx PUBLIC DART_SHARED_LIB)

if (IS_WIN_D)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (EXPORT_ALL_SYMBOL)
      target_link_options(mediaxx PRIVATE 
        "-Wl,--export-all-symbols"
      )
    else ()
      target_link_options(mediaxx PRIVATE 
        "-Wl,@${mediaxx_EXPORT_KEEP_SYMBOL}"
      )
    endif()
  endif()
elseif (IS_ANDROID_D)
  # Support Android 15 16k page size
  target_link_options(mediaxx PRIVATE 
    "-Wl,-z,max-page-size=16384"
    "-Wl,-rpath=\$ORIGIN"
  )
  if (EXPORT_ALL_SYMBOL)
    target_link_options(mediaxx PRIVATE 
      "-Wl,--export-dynamic,--whole-archive,--allow-multiple-definition"
    )
  else ()
    target_link_options(mediaxx PRIVATE 
      "-Wl,@${mediaxx_EXPORT_KEEP_SYMBOL}"
      "-Wl,--version-script=${mediaxx_EXPORT_IDS}"
    )
  endif()
elseif (IS_LINUX_D)
  target_link_options(mediaxx PRIVATE 
    "-Wl,-rpath=\$ORIGIN"
  )
  if (NOT DEFINED LUMENXX_BUILD_TYPE)
    # - 指定 LUMENXX_BUILD_TYPE 时不指定导出符号，此时可能只连接了ffmpeg，且为动态链接
    # 可能导致链接器报错 指定了需要的符号，却找不到符号实现 /usr/bin/ld: read in flex scanner failed
    if (EXPORT_ALL_SYMBOL)
      target_link_options(mediaxx PRIVATE 
        "-Wl,--export-dynamic,--whole-archive,--allow-multiple-definition"
      )
    else ()
      target_link_options(mediaxx PRIVATE 
        "-Wl,@${mediaxx_EXPORT_KEEP_SYMBOL}"
        "-Wl,--version-script=${mediaxx_EXPORT_IDS}"
      )
    endif()
  endif()
elseif (IS_IOS_D)
  target_link_options(mediaxx PRIVATE
    "-Wl,-rpath,@loader_path"
  )
  if (EXPORT_ALL_SYMBOL)
    target_link_options(mediaxx PRIVATE 
      "-Wl,-export_dynamic,-all_load,-m"
    )
  else ()
    target_link_options(mediaxx PRIVATE 
      "-Wl,@${mediaxx_EXPORT_KEEP_SYMBOL}"
      "-Wl,-exported_symbols_list,${mediaxx_EXPORT_IDS}"
    )
  endif()
elseif (IS_MACOS_D) 
  target_link_options(mediaxx PRIVATE 
    "-Wl,-rpath,@loader_path"
  )
  if (NOT DEFINED LUMENXX_BUILD_TYPE)
    if (EXPORT_ALL_SYMBOL)
      target_link_options(mediaxx PRIVATE 
        "-Wl,-export_dynamic,-all_load,-m"
      )
    else ()
      target_link_options(mediaxx PRIVATE 
        "-Wl,@${mediaxx_EXPORT_KEEP_SYMBOL}"
        "-Wl,-exported_symbols_list,${mediaxx_EXPORT_IDS}"
      )
    endif()
  endif()
endif()

if (DEFINED LUMENXX_BUILD_TYPE)
  ADD_SUBDIRECTORY(test)
  add_dependencies(test mediaxx)
endif()

if (DEFINED LUMENXX_BUILD_TYPE)
  install(TARGETS mediaxx DESTINATION "${EXE_INSTALL_PREFIX}")

  if (IS_WIN_D)
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/avcodec.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/avdevice.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/avfilter.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/avformat.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/avutil.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/swresample.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/swscale.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  elseif (IS_LINUX_D)
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libavcodec.so" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libavdevice.so" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libavfilter.so" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libavformat.so" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libavutil.so" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libswresample.so" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libswscale.so" DESTINATION "${EXE_INSTALL_PREFIX}")

    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libva.so.2" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libva-drm.so.2" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libva-x11.so.2" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libvdpau.so.1" DESTINATION "${EXE_INSTALL_PREFIX}")
  elseif (IS_MACOS_D)
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libavcodec.dylib" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libavdevice.dylib" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libavfilter.dylib" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libavformat.dylib" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libavutil.dylib" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libswresample.dylib" DESTINATION "${EXE_INSTALL_PREFIX}")
    install(FILES "${ffmpeg_mpv_BINARY_DIR}/libswscale.dylib" DESTINATION "${EXE_INSTALL_PREFIX}")
  endif ()
elseif (STATIC_LINK_FFMPEG)
  install(TARGETS mediaxx RUNTIME)
endif()
