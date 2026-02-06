cmake_minimum_required(VERSION 3.10)

project(MCPServer VERSION 1.0.0.1001)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

# 设置tinymcp项目根目录
set(TINYMCP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../")

set(ALL_SRC_FILES "")
file(GLOB_RECURSE EXAMPLE_SRC_FILES "../../Source/*.cpp")
list(APPEND ALL_SRC_FILES ${EXAMPLE_SRC_FILES})

add_executable(MCPServer ${ALL_SRC_FILES})

target_include_directories(MCPServer PRIVATE
    ${TINYMCP_ROOT}/Source/Protocol
)

target_link_libraries(MCPServer PRIVATE 
    tinymcp
    jsoncpp_static
)
