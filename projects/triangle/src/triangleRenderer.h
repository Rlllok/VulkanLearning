#pragma once

#include <device.h>
#include <swapchain.h>

#include "volk.h"

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#else
#include <GLFW/glfw3.h>
#endif

#include <string>

class TriangleRenderer
{
public:
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    TriangleRenderer(struct android_app* app, bool isDebug = false);
#else
    TriangleRenderer(bool isDebug = false);
#endif
    ~TriangleRenderer();
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    void initWindow(ANativeWindow* window);
#else
    void initWindow(uint32_t windowWidth, uint32_t windowHeight, std::string windowTitle);
#endif
    void initVulkan();
    void draw();
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    void startLoop(struct android_app* app);
#else
    void startLoop();
#endif
    bool isReady();

private:
    bool        isDebug = false;
    bool        isInitialized = false;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    ANativeWindow* window = nullptr;
    struct android_app* app;
#else
    GLFWwindow*  window = nullptr;
#endif

    VkInstance                      instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT        debugMessenger;
    Device*                         device = nullptr;
    SwapChain*                      swapchain = nullptr;
    VkRenderPass                    renderPass;
    std::vector<VkFramebuffer>      swapchainFramebuffers;
    VkPipelineLayout                graphicsPipelineLayout;
    VkPipeline                      graphicsPipeline;
    VkCommandPool                   cmdPool;
    std::vector<VkCommandBuffer>    cmdBuffers;
    std::vector<VkSemaphore>        imageAvailableSemaphores;
    std::vector<VkSemaphore>        renderFinishedSemaphores;
    std::vector<VkFence>            bufferFences;

    void createInstance();
    void createDevice();
    void createSwapchain();
    void createRenderPass();
    void createSwapchainFramebuffers();
    void createGraphicsPipeline();
    void createCmdPool();
    void createCmdBuffers();
    void recordCmdBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex);
    void createSyncTools();

    std::vector<const char*> getRequiredExtensions();
};