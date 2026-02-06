cmake_minimum_required(VERSION 3.10)

set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Use position independent code" FORCE)

set(JSONCPP_WITH_TESTS OFF CACHE BOOL "Disable jsoncpp tests" FORCE)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "Disable jsoncpp post build unittest" FORCE)
set(JSONCPP_WITH_EXAMPLE OFF CACHE BOOL "Disable jsoncpp example" FORCE)

add_subdirectory(External/jsoncpp)

set(CMAKE_CXX_STANDARD 17)

target_compile_features(jsoncpp_lib PRIVATE cxx_std_17)
target_compile_features(jsoncpp_static PRIVATE cxx_std_17)

set(TARGET_NAME "tinymcp")

option(BUILD_TINYMCP_SHARED "Build tinymcp as shared library" ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ALL_SRC_FILES "")
file(GLOB_RECURSE PROTOCOL_SRC_FILES "./Protocol/*/*.cpp")
list(APPEND ALL_SRC_FILES ${PROTOCOL_SRC_FILES})

if(BUILD_TINYMCP_SHARED)
    add_library(${TARGET_NAME} SHARED ${ALL_SRC_FILES})
else()
    add_library(${TARGET_NAME} STATIC ${ALL_SRC_FILES})
endif()

target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/External/jsoncpp/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/External/cpp-httplib>
)

target_link_libraries(${TARGET_NAME} PUBLIC jsoncpp_static)

