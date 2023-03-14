#pragma once

#include "device.h"
#include "swapchain.h"

#include "volk.h"

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#else
#include <GLFW/glfw3.h>
#endif

#include <vector>
#include <string>

class BaseRenderer
{
public:
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    BaseRenderer(struct android_app* app, bool isDebug = false);
    void initWindow(ANativeWindow* window);
    void startRenderLoop(struct android_app* app);
#else
    BaseRenderer(bool isDebug = false);
    void initWindow(uint32_t width, uint32_t height, std::string title);
    virtual void startRenderLoop();
#endif
    ~BaseRenderer();

    virtual void initVulkan();
    virtual void prepareRenderer();
    virtual void draw() = 0;
    virtual bool setupCompleted();

protected:
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    struct android_app* app = nullptr;
    ANativeWindow* window = nullptr;
#else
    GLFWwindow* window;
#endif

    bool isSetupCompleted = false;

    VkInstance                  instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT    debugMessenger = VK_NULL_HANDLE;
    Device*                     device = nullptr;
    SwapChain*                  swapchain = nullptr;
    std::vector<VkFramebuffer>  swapchainFramebuffers;
    VkRenderPass                renderPass = VK_NULL_HANDLE;

    virtual void createInstance();
    virtual void createDevice();
    virtual void createSwapchain();
    virtual void createRenderPass();
    virtual void createSwapchainFramebuffers();

private:
    bool isDebug;
};