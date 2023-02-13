#pragma once

#include "device.h"

#include "volk.h"
#include <GLFW/glfw3.h>

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

    void initSurface(GLFWwindow* window);
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
    VkExtent2D          chooseExtent(GLFWwindow* Window);
    void                createImageViews();
};