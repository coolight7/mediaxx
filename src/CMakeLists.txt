# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0097 NEW)

project(mediaxx_library VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("[mediaxx] Current Configure Type: ${CMAKE_CONFIGURATION_TYPES}")
message("[mediaxx] Current Build Type: ${CMAKE_BUILD_TYPE}")

set(STATIC_LINK_FFMPEG OFF CACHE STRING "是否编译时静态链接 ffmepg")
set(EXPORT_ALL_SYMBOL OFF CACHE STRING "导出所有符号")

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT STATIC_LINK_FFMPEG)
  # release 时不需要，由主程序提供静态链接ffmpeg的动态库
  message("[mediaxx] 跳过编译 libmediaxx ，期望由主程序提供预编译动态库")
  return()
endif()

if (MSVC)
		set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}    /utf-8 /MD /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE}  /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++23preview")
		set(CMAKE_C_FLAGS_PROFILE     "${CMAKE_C_FLAGS_PROFILE}    /utf-8 /MD /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS_PROFILE   "${CMAKE_CXX_FLAGS_PROFILE}  /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++23preview")
		set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}      /utf-8 /MD /wd4200 /std:c17")
		set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}    /utf-8 /MD /wd4200 /Zc:__cplusplus /std:c++23preview")

    # set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (EXPORT_ALL_SYMBOL)
      # 导出所有符号
      set(_MEDIAXX_C_FLAGS   "-fvisibility=default")
		  set(_MEDIAXX_CXX_FLAGS "-fvisibility=default")
    else ()
      # 只导出部分符号
      set(_MEDIAXX_C_FLAGS   "-fvisibility=hidden")
		  set(_MEDIAXX_CXX_FLAGS "-fvisibility=hidden")
    endif()

		set(CMAKE_C_FLAGS_RELEASE     "${CMAKE_C_FLAGS_RELEASE}   ${_MEDIAXX_C_FLAGS}   -Wall -fPIC -pipe -std=c17")
		set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} ${_MEDIAXX_CXX_FLAGS} -Wall -fPIC -pipe -std=c++23 -fno-exceptions -fno-rtti")
		set(CMAKE_C_FLAGS_PROFILE     "${CMAKE_C_FLAGS_PROFILE}   ${_MEDIAXX_C_FLAGS}   -Wall -fPIC -pipe -std=c17")
		set(CMAKE_CXX_FLAGS_PROFILE   "${CMAKE_CXX_FLAGS_PROFILE} ${_MEDIAXX_CXX_FLAGS} -Wall -fPIC -pipe -std=c++23 -fno-exceptions -fno-rtti")
		set(CMAKE_C_FLAGS_DEBUG       "${CMAKE_C_FLAGS_DEBUG}     ${_MEDIAXX_C_FLAGS}   -Wall -fPIC -pipe -std=c17 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
		set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS_DEBUG}   ${_MEDIAXX_CXX_FLAGS} -Wall -fPIC -pipe -std=c++23 -fno-exceptions -fno-rtti -fno-omit-frame-pointer -fno-optimize-sibling-calls")

    if (ANDROID) 
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

include(ExternalProject)
ExternalProject_Add(
  simdjson_repo
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/simdjson/"
  UPDATE_COMMAND ""
  CMAKE_ARGS 
      -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
      -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
      "-DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}"
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
      -DTARGET_ARCHITECTURE=${TARGET_ARCHITECTURE}
      -DBUILD_SHARED_LIBS=OFF
      -DSIMDJSON_BUILD_STATIC_LIB=ON
  INSTALL_COMMAND ""
)
ExternalProject_Get_Property(simdjson_repo SOURCE_DIR)
set(simdjson_SOURCE_DIR "${SOURCE_DIR}")
ExternalProject_Get_Property(simdjson_repo BINARY_DIR)
set(simdjson_INSTALL_DIR "${BINARY_DIR}/")

# - .vscode/c_cpp_properties.json 默认取 android
# - 两种模式，动态链接 ffmpeg 和 静态链接 ffmpeg
#   - 静态链接时需要各种 ffmpeg 的依赖库，因此只在发布 release 时上 docker 内编译，最终合并 ffmpeg 到 mediaxx 内整合为一个动态库
set(ffmpeg_CFLAGS "")
set(ffmpeg_INCLUDE_DIR ".")
set(ffmpeg_BINARY_DIR ".")
set(ffmpeg_LIBRARIES 
  avcodec
  avdevice
  avfilter
  avformat
  avutil
  swresample
  swscale
)
set(third_party_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")
set(vulkan_INCLUDE_DIR ".")
# - 编译 libmediaxx 动态库时，如果静态链接 ffmpeg，下面的文件指定了最终导出的符号
# - 应当包含：
#     - `mediaxx.h` 导出的符号
#     - `安卓JNI接口`
#     - `libmpv` 可以引用的，部分静态链接库符号也由嵌入 libmediaxx 的 ffmpeg 导出，被libmpv引用，可大幅减少`libmpv` 的体积
# - 限制符号导出可以大幅减少 `libmediaxx`，不建议包含标准库的导出符号，这可能导致内存管理混乱
# - 更新 `ffmpeg`、`mediaxx`、`libmpv` 时，需要检查更新符号表
set(ffmpeg_help_EXPORT_DEF "")
set(ffmpeg_help_EXPORT_IDS "")
set(ffmpeg_help_EXPORT_KEEP_SYMBOL "")
if (WIN32)
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

    set(ffmpeg_CFLAGS 
      ${AVCODEC_CFLAGS}
      ${AVDEVICE_CFLAGS}
      ${AVFILTER_CFLAGS}
      ${AVFORMAT_CFLAGS}
      ${AVUTIL_CFLAGS}
      ${SWRESAMPLE_CFLAGS}
      ${SWSCALE_CFLAGS}
    )
    set(ffmpeg_BINARY_DIR 
      ${AVCODEC_LIBRARY_DIRS}
      ${AVDEVICE_LIBRARY_DIRS}
      ${AVFILTER_LIBRARY_DIRS}
      ${AVFORMAT_LIBRARY_DIRS}
      ${AVUTIL_LIBRARY_DIRS}
      ${SWRESAMPLE_LIBRARY_DIRS}
      ${SWSCALE_LIBRARY_DIRS}
    )
    set(ffmpeg_LIBRARIES 
      ${AVCODEC_LIBRARIES}
      ${AVDEVICE_LIBRARIES}
      ${AVFILTER_LIBRARIES}
      ${AVFORMAT_LIBRARIES}
      ${AVUTIL_LIBRARIES}
      ${SWRESAMPLE_LIBRARIES}
      ${SWSCALE_LIBRARIES}
      avcodec
      avdevice
      avfilter
      avformat
      avutil
      swresample
      swscale
      m
      shlwapi
      pthread
      cfgmgr32
    )
    set(ffmpeg_INCLUDE_DIR 
      ${AVCODEC_INCLUDE_DIRS}
      ${AVDEVICE_INCLUDE_DIRS}
      ${AVFILTER_INCLUDE_DIRS}
      ${AVFORMAT_INCLUDE_DIRS}
      ${AVUTIL_INCLUDE_DIRS}
      ${SWRESAMPLE_INCLUDE_DIRS}
      ${SWSCALE_INCLUDE_DIRS}
    )
  else () 
    set(ffmpeg_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/libs-win/")
    set(ffmpeg_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/include-win/")
    set(vulkan_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan-header/include/")
  endif ()

  set(ffmpeg_help_EXPORT_DEF "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-win.def")
  set(ffmpeg_help_EXPORT_KEEP_SYMBOL "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-win-symbols.rsp")
elseif (DEFINED ANDROID)
  message("[mediaxx] build for android | ANDROID_ABI: ${ANDROID_ABI}")

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

    set(ffmpeg_CFLAGS 
      ${AVCODEC_CFLAGS}
      ${AVDEVICE_CFLAGS}
      ${AVFILTER_CFLAGS}
      ${AVFORMAT_CFLAGS}
      ${AVUTIL_CFLAGS}
      ${SWRESAMPLE_CFLAGS}
      ${SWSCALE_CFLAGS}
    )
    set(ffmpeg_BINARY_DIR 
      ${AVCODEC_LIBRARY_DIRS}
      ${AVDEVICE_LIBRARY_DIRS}
      ${AVFILTER_LIBRARY_DIRS}
      ${AVFORMAT_LIBRARY_DIRS}
      ${AVUTIL_LIBRARY_DIRS}
      ${SWRESAMPLE_LIBRARY_DIRS}
      ${SWSCALE_LIBRARY_DIRS}
    )
    set(ffmpeg_LIBRARIES 
      ${AVCODEC_LIBRARIES}
      ${AVDEVICE_LIBRARIES}
      ${AVFILTER_LIBRARIES}
      ${AVFORMAT_LIBRARIES}
      ${AVUTIL_LIBRARIES}
      ${SWRESAMPLE_LIBRARIES}
      ${SWSCALE_LIBRARIES}
      avcodec
      avdevice
      avfilter
      avformat
      avutil
      swresample
      swscale
    )
    set(ffmpeg_INCLUDE_DIR 
      ${AVCODEC_INCLUDE_DIRS}
      ${AVDEVICE_INCLUDE_DIRS}
      ${AVFILTER_INCLUDE_DIRS}
      ${AVFORMAT_INCLUDE_DIRS}
      ${AVUTIL_INCLUDE_DIRS}
      ${SWRESAMPLE_INCLUDE_DIRS}
      ${SWSCALE_INCLUDE_DIRS}
    )
  else ()
    set(ffmpeg_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/libs-android/${ANDROID_ABI}/")
    set(ffmpeg_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/include-android/")
  endif ()

  set(ffmpeg_help_EXPORT_IDS "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-android.lds")
  set(ffmpeg_help_EXPORT_KEEP_SYMBOL "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-help/libmpv-android-symbols.txt")
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/)
include_directories(${simdjson_SOURCE_DIR}/include/)
include_directories(${vulkan_INCLUDE_DIR})
include_directories(${third_party_DIR})
include_directories(${ffmpeg_INCLUDE_DIR})

aux_source_directory("./lib/util" DIR_UTIL)
aux_source_directory("./lib/analyse" DIR_ANALYSE)
aux_source_directory("./impl" DIR_IMPL)
aux_source_directory("./impl/android/" DIR_IMPL_ANDROID)

add_library(mediaxx SHARED
  ${DIR_IMPL}
  ${DIR_IMPL_ANDROID}
  ${DIR_UTIL}
  ${DIR_ANALYSE}

  ${ffmpeg_help_EXPORT_DEF}
)

add_dependencies(mediaxx simdjson_repo)

target_link_directories(mediaxx PUBLIC 
  ${ffmpeg_BINARY_DIR}
  ${simdjson_INSTALL_DIR}
)

target_link_libraries(mediaxx PUBLIC
  simdjson
  ${ffmpeg_LIBRARIES}
)

target_compile_options(mediaxx PRIVATE 
    ${ffmpeg_CFLAGS}
)

set_target_properties(mediaxx PROPERTIES
  PUBLIC_HEADER mediaxx.h
  PREFIX "lib"
  OUTPUT_NAME "mediaxx"
)

target_compile_definitions(mediaxx PUBLIC DART_SHARED_LIB)

if (WIN32)
  if (EXPORT_ALL_SYMBOL)
    target_link_options(mediaxx PRIVATE 
      "-Wl,--export-all-symbols"
    )
  else ()
    target_link_options(mediaxx PRIVATE 
      "-Wl,@${ffmpeg_help_EXPORT_KEEP_SYMBOL}"
    )
  endif()
elseif (ANDROID)
  # Support Android 15 16k page size
  target_link_options(mediaxx PRIVATE 
    "-Wl,-z,max-page-size=16384"
  )
  if (EXPORT_ALL_SYMBOL)
    target_link_options(mediaxx PRIVATE 
      "-Wl,--export-dynamic,--whole-archive,--allow-multiple-definition"
    )
  else ()
    target_link_options(mediaxx PRIVATE 
      "-Wl,@${ffmpeg_help_EXPORT_KEEP_SYMBOL}"
      "-Wl,--version-script=${ffmpeg_help_EXPORT_IDS}"
    )
  endif()
endif()

if (DEFINED LUMENXX_BUILD_TYPE)
  ADD_SUBDIRECTORY(test)
  add_dependencies(test mediaxx)
endif()

if (DEFINED LUMENXX_BUILD_TYPE)
  install(TARGETS mediaxx RUNTIME DESTINATION "${EXE_INSTALL_PREFIX}")

  install(FILES "${ffmpeg_BINARY_DIR}/avcodec.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/avdevice.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/avfilter.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/avformat.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/avutil.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/swresample.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
  install(FILES "${ffmpeg_BINARY_DIR}/swscale.dll" DESTINATION "${EXE_INSTALL_PREFIX}")
elseif (STATIC_LINK_FFMPEG)
  install(TARGETS mediaxx RUNTIME)
endif()
