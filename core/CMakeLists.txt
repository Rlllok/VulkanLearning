file(GLOB BASE_SOURCES "*.cpp")
file(GLOB BASE_HEADERS "*.h")

add_library(core STATIC ${BASE_SOURCES} ${BASE_HEADERS})
target_link_libraries(core Vulkan::Vulkan glfw)