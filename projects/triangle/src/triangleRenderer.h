#pragma once

#include "window.h"
#include "device.h"
#include "swapchain.h"

#include <vulkan/vulkan.h>

#include <string>

class TriangleRenderer
{
public:

    TriangleRenderer(bool isDebug = false);
    ~TriangleRenderer();
    void start(uint32_t windowWidth, uint32_t windowHeight, std::string windowTitle);

private:
    bool    isDebug;
    Window* window = nullptr;

    VkInstance                  instance;
    VkDebugUtilsMessengerEXT    debugMessenger;
    Device*                     device = nullptr;
    SwapChain*                  swapchain = nullptr;

    void initVulkan();
    void createInstance();
    void createDevice();
    void createSwapchain();
    void createRenderPass();
};