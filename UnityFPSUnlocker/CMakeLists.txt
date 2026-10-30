# CMakeList.txt: UnityFPSUnlocker 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.21)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS")

set(LibraryName UnityFPSUnlocker)

# 隐藏导出函数
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# 添加Debug和Release版本信息
string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_UP)
if(CMAKE_BUILD_TYPE_UP STREQUAL DEBUG)
    message(STATUS "Debug Version")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -g")
else()
    message(STATUS "Release Version")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Oz -flto -fdata-sections -ffunction-sections")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Oz -flto -fdata-sections -ffunction-sections")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -Wl,--exclude-libs,ALL -flto")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s -Wl,--exclude-libs,ALL -Wl,--gc-sections -flto")
endif()

# Preserve caller-provided cross-compiled dependency prefixes (e.g. Abseil).
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SYSROOT}/${CMAKE_LIBRARY_ARCHITECTURE}")
message(STATUS CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH})

# 添加头文件
include_directories(
    .
    "third/rapidjson"
    "third/xdl"
)

# 文件写出位置
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/libs/${CMAKE_BUILD_TYPE}/)

# 将源代码添加到此项目的可执行文件。
aux_source_directory(${PROJECT_SOURCE_DIR}/${LibraryName} program-src)
aux_source_directory(unity unity-src)
aux_source_directory(utility utility-src)
aux_source_directory(third/xdl xdl-src)
aux_source_directory(third/riru_hide riru_hide-src)
aux_source_directory(file_watch listener-src)
aux_source_directory(file_watch/dispatcher dispatcher-src)

add_library (
    ${LibraryName}
    SHARED
    ${program-src}
    ${unity-src}
    ${utility-src}
    ${listener-src}
    ${dispatcher-src}
    ${xdl-src}
    ${riru_hide-src}
)

find_library(log-lib log)
target_link_libraries(${LibraryName} ${log-lib})

find_package(absl REQUIRED)
target_link_libraries(${LibraryName} absl::status)
target_link_libraries(${LibraryName} absl::statusor)
target_link_libraries(${LibraryName} absl::flat_hash_set)

# ARM64-only runtime FPS setter interception. A fixed upstream Dobby revision
# is cloned by CI and linked statically; no extra runtime APK/.so is required.
if(ANDROID AND ANDROID_ABI STREQUAL "arm64-v8a")
    if(NOT DEFINED DOBBY_DIR OR NOT EXISTS "${DOBBY_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "DOBBY_DIR must point to pinned Dobby source for ARM64")
    endif()
    set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
    set(DOBBY_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(Plugin.SymbolResolver OFF CACHE BOOL "" FORCE)
    add_subdirectory("${DOBBY_DIR}" "${CMAKE_BINARY_DIR}/dobby-build")
    target_link_libraries(${LibraryName} dobby_static)
endif()

set_target_properties(${LibraryName} PROPERTIES PREFIX "")
set_target_properties(${LibraryName} PROPERTIES OUTPUT_NAME ${ANDROID_ABI})