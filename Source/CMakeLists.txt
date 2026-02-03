cmake_minimum_required(VERSION 3.10)

# 设置jsoncpp为静态库
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build jsoncpp as static library" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "Build jsoncpp as static library" FORCE)
# 确保jsoncpp使用位置无关代码编译，以便能链接到动态库
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Use position independent code" FORCE)

add_subdirectory(External/jsoncpp)

set(TARGET_NAME "tinymcp")

# 添加选项控制编译为静态库还是动态库
option(BUILD_TINYMCP_SHARED "Build tinymcp as shared library" ON)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ALL_SRC_FILES "")
file(GLOB_RECURSE PROTOCOL_SRC_FILES "./Protocol/*/*.cpp")
list(APPEND ALL_SRC_FILES ${PROTOCOL_SRC_FILES})

# 根据选项决定编译为静态库还是动态库
if(BUILD_TINYMCP_SHARED)
    add_library(${TARGET_NAME} SHARED ${ALL_SRC_FILES})
else()
    add_library(${TARGET_NAME} STATIC ${ALL_SRC_FILES})
endif()

# 添加jsoncpp头文件目录
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/External/jsoncpp/include)

# 链接jsoncpp静态库
target_link_libraries(${TARGET_NAME} PRIVATE jsoncpp_static)