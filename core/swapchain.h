#pragma once

#include "device.h"
#include "window.h"

#include <vulkan/vulkan.h>

class SwapChain
{
public:
    SwapChain(VkInstance instance, Device* device);
    ~SwapChain();

    // Getters
    uint32_t getImageCount() { return imageCount; }

    void initSurface(Window* window);
    void initSwapchain();

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

    void findSupportDetails();
    VkSurfaceFormatKHR chooseSurfaceFormat();
    VkPresentModeKHR choosePresentMode();
    VkExtent2D chooseExtent(Window* Window);
    void createImageViews();
};