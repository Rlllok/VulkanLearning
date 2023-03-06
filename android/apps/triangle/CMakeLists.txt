cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project("triangleApp")
set(NAME triangle)

set(SRC_DIR ../../../projects/${NAME}/src)
set(CORE_DIR ../../../core)
set(EXTERNAL_DIR ../../../externals)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_ANDROID_KHR -DVK_NO_PROTOTYPES")

file(GLOB PROJECT_SRC "${SRC_DIR}/*.cpp")

set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_ANDROID_KHR)
add_subdirectory(${EXTERNAL_DIR}/volk ${CMAKE_BINARY_DIR}/bin)
add_subdirectory(../core ${CMAKE_SOURCE_DIR}/../core)

add_library(native-lib SHARED ${PROJECT_SRC})
add_library(native-app-glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

include_directories(${CORE_DIR})
include_directories(${EXTERNAL_DIR}/glm)
include_directories(${ANDROID_NDK}/sources/android/native_app_glue)

target_link_libraries(
    native-lib
    native-app-glue
    libcore
    android
    log
    z
    volk
)