#include "swapchain.h"
#include "errorLog.h"

#include <algorithm>

SwapChain::SwapChain(VkInstance instance, Device* device)
{
    this->instance = instance;
    this->device = device;
}

SwapChain::~SwapChain()
{
    for (uint32_t i = 0; i < imageCount; i++) {
        vkDestroyImageView(device->getLogicalDevice(), imageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(device->getLogicalDevice(), swapchain, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);
}

void SwapChain::initSwapchain()
{
    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;

    uint32_t imgCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imgCount > capabilities.maxImageCount) {
        imgCount = capabilities.maxImageCount;
    }
    swapchainInfo.minImageCount = imgCount;
    swapchainInfo.imageFormat = chooseSurfaceFormat().format;
    swapchainInfo.imageColorSpace = chooseSurfaceFormat().colorSpace;
    swapchainInfo.imageExtent = swapchainExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.pQueueFamilyIndices = nullptr;
    swapchainInfo.preTransform = capabilities.currentTransform;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
#else
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
#endif
    swapchainInfo.presentMode = choosePresentMode();
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(device->getLogicalDevice(), &swapchainInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS) {
        error::log("cannot create Swapchain.");
    }

    imageCount = 0;
    vkGetSwapchainImagesKHR(device->getLogicalDevice(), swapchain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(device->getLogicalDevice(), swapchain, &imageCount, images.data());

    createImageViews();
}

VkResult SwapChain::acquireNextImage(VkSemaphore semaphore, uint32_t *imageIndex)
{
    return vkAcquireNextImageKHR(
        device->getLogicalDevice(),
        swapchain,
        UINT64_MAX,
        semaphore,
        nullptr,
        imageIndex
    );
}

VkFormat SwapChain::getImageFormat()
{
    return chooseSurfaceFormat().format;
}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void SwapChain::initSurface(ANativeWindow* window)
#else
void SwapChain::initSurface(GLFWwindow* window)
#endif
{
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    VkAndroidSurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = window;

    VkResult result = vkCreateAndroidSurfaceKHR(instance, &surfaceInfo, nullptr, &surface);
#else
    VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
#endif
    if (result != VK_SUCCESS) {
        error::log("cannot create Surface.");
    }

    findSupportDetails();

    swapchainExtent = chooseExtent(window);
}

void SwapChain::findSupportDetails()
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->getPhysicalDevice(), surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device->getPhysicalDevice(), surface, &formatCount, nullptr);
    if (formatCount == 0) {
        error::log("cannot find any supported Surface Formats.");
    }

    surfaceFormats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device->getPhysicalDevice(), surface, &formatCount, surfaceFormats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device->getPhysicalDevice(), surface, &presentModeCount , nullptr);
    if (presentModeCount == 0) {
        error::log("cannot find any supported Present Mode.");
    }

    presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device->getPhysicalDevice(), surface, &presentModeCount, presentModes.data());
}

VkSurfaceFormatKHR SwapChain::chooseSurfaceFormat()
{
    for (const auto& format : surfaceFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return surfaceFormats[0];
}

VkPresentModeKHR SwapChain::choosePresentMode()
{
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
VkExtent2D SwapChain::chooseExtent(ANativeWindow* window)
#else
VkExtent2D SwapChain::chooseExtent(GLFWwindow* window)
#endif
{
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    return capabilities.currentExtent;
#else
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	else {
		int width;
		int height;

		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D extent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.width);

		return extent;
	}
#endif
}

void SwapChain::createImageViews()
{
    imageViews.resize(imageCount);

    for (int i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = chooseSurfaceFormat().format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseMipLevel = 0;

        VkResult result = vkCreateImageView(device->getLogicalDevice(), &viewInfo, nullptr, &imageViews[i]);
        if (result != VK_SUCCESS) {
            error::log("cannot create Swapchain Image View.");
        }
    }
}