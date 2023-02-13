#pragma once

#include "device.h"
#include "swapchain.h"

// #define VOLK_IMPLEMENTATION
#include "volk.h"
#include <GLFW/glfw3.h>

#include <string>

class TriangleRenderer
{
public:
    TriangleRenderer(bool isDebug = false);
    ~TriangleRenderer();
    void initWindow(uint32_t windowWidth, uint32_t windowHeight, std::string windowTitle);
    void initVulkan();
    void draw();
    void startLoop();

private:
    bool        isDebug;
    GLFWwindow*  window;

    VkInstance                      instance;
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