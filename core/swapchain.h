#pragma once

#include "device.h"

#include "volk.h"

#ifdef VK_USE_PLATFORM_ANDROID_KHR
#include <android/native_window.h>
#else
#include <GLFW/glfw3.h>
#endif

class SwapChain
{
public:
    SwapChain(VkInstance instance, Device* device);
    ~SwapChain();

    operator VkSwapchainKHR() const { return swapchain; }

    // Getters
    uint32_t                    getImageCount()     { return imageCount; }
    std::vector<VkImageView>    getImageViews()     { return imageViews; }
    VkExtent2D                  getExtent()         { return swapchainExtent; }
    VkFormat                    getImageFormat();

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    void initSurface(ANativeWindow* window);
#else
    void initSurface(GLFWwindow* window);
#endif
    void initSwapchain();

    VkResult acquireNextImage(VkSemaphore semaphore, uint32_t* imageIndex);

private:

    VkInstance                      instance;
    Device*                         device = nullptr;
    VkSwapchainKHR                  swapchain;
    VkSurfaceKHR                    surface;
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    std::vector<VkPresentModeKHR>   presentModes;
    VkExtent2D                      swapchainExtent;
    std::vector<VkImage>            images;
    std::vector<VkImageView>        imageViews;
    uint32_t                        imageCount = 0;

    void                findSupportDetails();
    VkSurfaceFormatKHR  chooseSurfaceFormat();
    VkPresentModeKHR    choosePresentMode();
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    VkExtent2D          chooseExtent(ANativeWindow* Window);
#else
    VkExtent2D          chooseExtent(GLFWwindow* Window);
#endif
    void                createImageViews();
};