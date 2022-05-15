cmake_minimum_required(VERSION 3.0)

set(CMAKE_BUILD_TYPE Debug)

set(EDITOR_NAME "paphos")
set(SOURCE_PATH "src")

add_compile_definitions(
    EDITOR_NAME="${EDITOR_NAME}"
    FLECS_THREAD_COUNT=8
    ENABLE_VALIDATION_LAYERS=true)

project(${EDITOR_NAME})

set(CMAKE_BUILD_TYPE "Debug")

include_directories("deps")
include_directories("deps/glm")
include_directories("deps/flecs/include")
include_directories("deps/glfw/include")
include_directories("deps/spdlog/include")

file(GLOB_RECURSE SOURCES "${SOURCE_PATH}/*.cpp")

add_subdirectory("deps/flecs")
add_subdirectory("deps/glfw")
add_subdirectory("deps/glm")

find_package(Vulkan REQUIRED)

add_executable(${EDITOR_NAME} ${SOURCES})
target_link_libraries(${EDITOR_NAME} glfw flecs vulkan)