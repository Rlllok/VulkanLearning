cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(VulkanLearning)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DVK_NO_PROTOTYPES)

# Volk
add_subdirectory(${CMAKE_SOURCE_DIR}/externals/volk)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/externals/glfw)

# GLM
include_directories(${CMAKE_SOURCE_DIR}/externals/glm)

# STB
include_directories(${CMAKE_SOURCE_DIR}/externals/stb)

# tinyobjloader
include_directories(${CMAKE_SOURCE_DIR}/externals/tinyobjloader)

# my
include_directories(${CMAKE_SOURCE_DIR}/core)
add_subdirectory(core)

add_compile_definitions(MODELS_DIR=\"${CMAKE_SOURCE_DIR}/models/\")
add_compile_definitions(TEXTURES_DIR=\"${CMAKE_SOURCE_DIR}/textures/\")

function(buildProject PROJECT_DIR NAME)

    file(GLOB SOURCE_FILES
            ${PROJECT_DIR}/src/*.c
            ${PROJECT_DIR}/src/*.cpp)

    file(GLOB HEADER_FILES
            ${PROJECT_DIR}/src/*.h
            ${PROJECT_DIR}/src/*.hpp)

    file(GLOB SHADER_FILES
            ${PROJECT_DIR}/shaders/*.spv)

    file(COPY ${PROJECT_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(${NAME} ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES})
    target_link_libraries(${NAME} volk_headers glfw core)
endfunction(buildProject)

add_subdirectory(projects)