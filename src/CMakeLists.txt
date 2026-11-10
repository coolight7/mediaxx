# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0097 NEW)

project(mediaxx_library VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XX_BUILD_TYPE "Generate the client target." "RELEASE")

# 仅用于指示是否编译用于调试用途，不代表真实编译类型
# - [IS_DEBUG_D] == 1 时，启用大部分调试日志等信息，因此 profile 之类注重
# 性能的编译将视为 release
set(IS_DEBUG_D    0)
set(IS_RELEASE_D  1)

if (XX_BUILD_TYPE STREQUAL "DEBUG")
  set(IS_DEBUG_D    1)
  set(IS_RELEASE_D  0)
endif()
add_definitions(-DXX_BUILD_TYPE=${XX_BUILD_TYPE})

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

# ===== 依赖库: cxx_utilxx_base / cxx_utilxx =====
# 布局与 musicxx_extern_plugin 一致: 源码来自 src/third_party 下的 git 子模块。
# 但这里用 FetchContent + add_subdirectory, 不用那套 ExternalProject 超级构建:
# mediaxx 的 CMake 会被 Flutter 平台构建和 libmpv-*-build 仓库直接调用,
# 必须在本工程里产出 mediaxx 目标 (父工程要拿 $<TARGET_FILE:mediaxx>)。
include(FetchContent)

# 依赖产物 (生成的 Boost 配置、包声明) 统一放这里, 不污染源码树
set(_mediaxx_dep_prefix "${CMAKE_BINARY_DIR}/mediaxx-deps")

# ---- Boost (仅头文件) ----
# cxx_utilxx_base / cxx_utilxx 用 find_package(Boost CONFIG) 且只取头文件根
# (源码写 <asio/...> 时靠 <头文件根>/boost/ 命中 Boost.Asio), 不链接 Boost 编译库。
# 头文件来源: -DMEDIAXX_BOOST_INCLUDE=<含 boost/ 的目录> → src/third_party/boost/include
#            → .native/boost/boost_*
set(MEDIAXX_BOOST_INCLUDE "" CACHE PATH "含 boost/ 子目录的 Boost 头文件根 (只用头文件)")
if (NOT MEDIAXX_BOOST_INCLUDE)
  set(_mediaxx_boost_candidates "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost/include")
  file(GLOB _mediaxx_boost_cached "${CMAKE_CURRENT_SOURCE_DIR}/../.native/boost/boost_*")
  list(APPEND _mediaxx_boost_candidates ${_mediaxx_boost_cached})
  foreach (_cand ${_mediaxx_boost_candidates})
    if (EXISTS "${_cand}/boost/version.hpp")
      set(MEDIAXX_BOOST_INCLUDE "${_cand}")
      break()
    endif ()
  endforeach ()
  unset(_mediaxx_boost_candidates)
  unset(_mediaxx_boost_cached)
endif ()
if (NOT MEDIAXX_BOOST_INCLUDE OR NOT EXISTS "${MEDIAXX_BOOST_INCLUDE}/boost/version.hpp")
  message(FATAL_ERROR
    "[mediaxx] 缺少 Boost 头文件 (依赖库只用头文件, 不链接 Boost 编译库)\n"
    "  解包后指定: -DMEDIAXX_BOOST_INCLUDE=<含 boost/ 的目录>\n"
    "  或放到 src/third_party/boost/include (该目录不进仓库)")
endif ()
file(TO_CMAKE_PATH "${MEDIAXX_BOOST_INCLUDE}" MEDIAXX_BOOST_INCLUDE)
message("[mediaxx] Boost 头文件: ${MEDIAXX_BOOST_INCLUDE}")

# 版本号直接从 boost/version.hpp 读 (BOOST_VERSION 形如 109200 = 1.92.0), 作为唯一版本来源
file(STRINGS "${MEDIAXX_BOOST_INCLUDE}/boost/version.hpp" _mediaxx_boost_ver_line
     REGEX "^#define BOOST_VERSION ")
string(REGEX MATCH "[0-9]+" _mediaxx_boost_code "${_mediaxx_boost_ver_line}")
if (NOT _mediaxx_boost_code)
  message(FATAL_ERROR
    "[mediaxx] 无法从 ${MEDIAXX_BOOST_INCLUDE}/boost/version.hpp 读出 BOOST_VERSION")
endif ()
math(EXPR _mediaxx_boost_major "${_mediaxx_boost_code} / 100000")
math(EXPR _mediaxx_boost_minor "(${_mediaxx_boost_code} / 100) % 1000")
math(EXPR _mediaxx_boost_patch "${_mediaxx_boost_code} % 100")
set(MEDIAXX_BOOST_VERSION "${_mediaxx_boost_major}.${_mediaxx_boost_minor}.${_mediaxx_boost_patch}")
unset(_mediaxx_boost_ver_line)
unset(_mediaxx_boost_code)
unset(_mediaxx_boost_major)
unset(_mediaxx_boost_minor)
unset(_mediaxx_boost_patch)

# 生成 BoostConfig.cmake: b2 那套完整配置用不上 (只要头文件根), 而且预生成的配置会把
# 绝对路径烘焙进导出目标; 依赖库的 find_package(Boost CONFIG) 从这里取
set(BOOST_INCLUDE_DIR "${MEDIAXX_BOOST_INCLUDE}")
set(BOOST_VERSION "${MEDIAXX_BOOST_VERSION}")
set(_mediaxx_boost_cfg_dir
    "${_mediaxx_dep_prefix}/boost/lib/cmake/Boost-${MEDIAXX_BOOST_VERSION}")
file(MAKE_DIRECTORY "${_mediaxx_boost_cfg_dir}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BoostConfig.cmake.in"
               "${_mediaxx_boost_cfg_dir}/BoostConfig.cmake" @ONLY)
list(PREPEND CMAKE_PREFIX_PATH "${_mediaxx_dep_prefix}/boost")

# ---- OpenSSL (cxx_utilxx 的客户端 TLS 与散列) ----
# 平台没自带时用 -DMEDIAXX_OPENSSL_ROOT=<安装树> 指定; 有静态库时优先静态
# (产物不额外带 libssl/libcrypto, 与 musicxx_extern_plugin 的做法一致)
set(MEDIAXX_OPENSSL_ROOT "" CACHE PATH "OpenSSL 安装树 (含 include/ 与 lib/), 为空时用系统 OpenSSL")
if (MEDIAXX_OPENSSL_ROOT)
  file(TO_CMAKE_PATH "${MEDIAXX_OPENSSL_ROOT}" MEDIAXX_OPENSSL_ROOT)
  set(OPENSSL_ROOT_DIR "${MEDIAXX_OPENSSL_ROOT}" CACHE PATH "OpenSSL 安装树" FORCE)
  set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "OpenSSL 静态库优先" FORCE)
endif ()

# ---- fmt / simdjson / uchardet / iconv / html2md ----
# cxx_utilxx_base 用 find_package 取这四个依赖。源码树里没有安装好的 config,
# 所以下面按 find_package 的搜索规则各写一个最小 config (声明包可用并校验目标),
# 再把对应目录加进 CMAKE_PREFIX_PATH —— 不依赖 CMake 3.24 的 OVERRIDE_FIND_PACKAGE
# (Android 侧 Gradle 自带的 CMake 版本偏旧)。
# iconv 用 libiconv-native (提供 CMake 包与 Iconv::iconv), 不再下载 autotools 源码。
FetchContent_Declare(
  simdjson
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/simdjson/"
  EXCLUDE_FROM_ALL
)
block()
  set(BUILD_SHARED_LIBS             OFF)
  set(BUILD_STATIC_LIBS             ON)
  FetchContent_MakeAvailable(simdjson)
endblock()

FetchContent_Declare(
  fmt
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/"
)
block()
  set(FMT_PEDANTIC          OFF)
  set(FMT_INSTALL           OFF)
  set(FMT_WERROR            OFF)
  set(BUILD_SHARED_LIBS     OFF)
  set(BUILD_STATIC_LIBS     ON )
  set(FMT_DOC               OFF)
  set(FMT_TEST              OFF)
  FetchContent_MakeAvailable(fmt)
endblock()

FetchContent_Declare(
  uchardet
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/uchardet/"
  EXCLUDE_FROM_ALL
)
block()
  set(CHECK_SSE2            ON)
  set(BUILD_BINARY          OFF)
  set(BUILD_SHARED_LIBS     OFF)
  set(BUILD_STATIC_LIBS     ON )
  FetchContent_MakeAvailable(uchardet)
endblock()

FetchContent_Declare(
  iconv
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libiconv-native/"
)
block()
  set(ICONV_ENABLE_EXTRA    OFF)
  set(ICONV_EMBED_MODE      OFF)
  set(BUILD_SHARED_LIBS     OFF)
  FetchContent_MakeAvailable(iconv)
endblock()

FetchContent_Declare(
  html2md
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/html2md/"
  EXCLUDE_FROM_ALL
)
block()
  set(BUILD_EXE               OFF)
  set(BUILD_DOC               OFF)
  set(BUILD_TEST              OFF)
  set(PYTHON_BINDINGS         OFF)
  set(BUILD_SHARED_LIBS       OFF)
  FetchContent_MakeAvailable(html2md)
endblock()

# 源码树里的目标名与安装后 config 的名字不一样, 补别名对齐依赖库的写法
if (NOT TARGET uchardet::libuchardet AND TARGET libuchardet)
  add_library(uchardet::libuchardet ALIAS libuchardet)
endif ()
if (NOT TARGET Iconv::iconv AND TARGET iconv)
  add_library(Iconv::iconv ALIAS iconv)
endif ()
if (NOT TARGET Iconv::charset AND TARGET charset)
  add_library(Iconv::charset ALIAS charset)
endif ()

# 每个依赖写一个最小 config, 放在 <构建目录>/mediaxx-deps/<包名>/lib/cmake/<包名>/
function(_mediaxx_write_dep_config pkg target)
  # 目录名带 pkg/ 前缀: 不能和依赖库自己的构建目录 (${_mediaxx_dep_prefix}/${pkg}) 重合,
  # 否则会先命中它们为自己安装树生成的 config (那份要配 Targets 文件, 构建目录里没有)
  set(_dir "${_mediaxx_dep_prefix}/pkg/${pkg}/lib/cmake/${pkg}")
  file(MAKE_DIRECTORY "${_dir}")
  file(WRITE "${_dir}/${pkg}Config.cmake"
"# 由 mediaxx/src/CMakeLists.txt 生成: 源码树里已建好目标, 这里只声明包可用\n"
"if (NOT TARGET ${target})\n"
"  message(FATAL_ERROR \"[mediaxx] 依赖 ${pkg} 的目标 ${target} 不存在 (FetchContent 没有生效?)\")\n"
"endif ()\n"
"set(${pkg}_FOUND TRUE)\n")
endfunction()

_mediaxx_write_dep_config(fmt      "fmt::fmt")
_mediaxx_write_dep_config(simdjson "simdjson::simdjson")
_mediaxx_write_dep_config(uchardet "uchardet::libuchardet")
_mediaxx_write_dep_config(iconv    "Iconv::iconv")
_mediaxx_write_dep_config(html2md  "html2md")
# cxx_utilxx 用 find_package(cxx_utilxx_base) 找依赖库本体; 这里是同一个工程的
# add_subdirectory, 目标已经存在, 缺的只是一个能命中的包声明
_mediaxx_write_dep_config(cxx_utilxx_base "cxx_utilxx_base_static")
list(PREPEND CMAKE_PREFIX_PATH
  "${_mediaxx_dep_prefix}/pkg/fmt"
  "${_mediaxx_dep_prefix}/pkg/simdjson"
  "${_mediaxx_dep_prefix}/pkg/uchardet"
  "${_mediaxx_dep_prefix}/pkg/iconv"
  "${_mediaxx_dep_prefix}/pkg/html2md"
  "${_mediaxx_dep_prefix}/pkg/cxx_utilxx_base"
)

# ---- 依赖库本体 ----
# 依赖库的源码需要异常与 RTTI, 而 mediaxx 自己的目标要 -fno-exceptions/-fno-rtti:
# 配置 flags 按目录作用域生效, 进依赖段前临时去掉这两项, 出依赖段后恢复
# (mediaxx 自己的目标在恢复之后创建, 仍是不带异常与 RTTI 的写法)。
foreach (_cfg "" _DEBUG _RELEASE _PROFILE _RELWITHDEBINFO _MINSIZEREL)
  set(_mediaxx_saved_flags_${_cfg} "${CMAKE_CXX_FLAGS${_cfg}}")
  string(REPLACE "-fno-exceptions" "" _mediaxx_flags "${CMAKE_CXX_FLAGS${_cfg}}")
  string(REPLACE "-fno-rtti"       "" _mediaxx_flags "${_mediaxx_flags}")
  set(CMAKE_CXX_FLAGS${_cfg} "${_mediaxx_flags}")
endforeach ()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/cxx_utilxx_base"
                 "${_mediaxx_dep_prefix}/cxx_utilxx_base")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/cxx_utilxx"
                 "${_mediaxx_dep_prefix}/cxx_utilxx")

foreach (_cfg "" _DEBUG _RELEASE _PROFILE _RELWITHDEBINFO _MINSIZEREL)
  set(CMAKE_CXX_FLAGS${_cfg} "${_mediaxx_saved_flags_${_cfg}}")
  unset(_mediaxx_saved_flags_${_cfg})
endforeach ()
unset(_mediaxx_flags)
unset(_cfg)

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
# 依赖库 (fmt / simdjson / uchardet / iconv / cxx_utilxx_base / cxx_utilxx) 的 include
# 目录由它们自己的目标接口传递, 这里只列本工程的目录
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

target_link_directories(mediaxx PUBLIC 
  ${ffmpeg_mpv_BINARY_DIR}
)

# 基础工具库 (日志 / 字符串与编码 / JSON / 取消令牌) 与网络散列库。
# 它们的依赖 (fmt / simdjson / uchardet / iconv / Boost / OpenSSL / html2md)
# 都是 PUBLIC 传递; fmt 与 simdjson 再显式列一次, 因为 mediaxx 自己的代码
# 直接 include 它们的头 (media_info / chorus_analyse 用 simdjson 拼 JSON)。
target_link_libraries(mediaxx PUBLIC
  ${ffmpeg_mpv_LIBRARIES}
  cxx_utilxx_base_static
  cxx_utilxx_static
  fmt::fmt
  simdjson::simdjson
)

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
      "-Wl,--exclude-libs,ALL"
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
