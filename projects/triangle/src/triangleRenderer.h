#pragma once

#include "baseRenderer.h"
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
#include <vector>

class TriangleRenderer : public BaseRenderer
{
public:
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    TriangleRenderer(struct android_app* app, bool isDebug = false);
#else
    TriangleRenderer(bool isDebug = false);
#endif
    ~TriangleRenderer();
    
    void prepareRenderer() override;

private:
    VkPipelineLayout                graphicsPipelineLayout = VK_NULL_HANDLE;
    VkPipeline                      graphicsPipeline = VK_NULL_HANDLE;
    VkCommandPool                   cmdPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>    cmdBuffers;
    std::vector<VkSemaphore>        imageAvailableSemaphores;
    std::vector<VkSemaphore>        renderFinishedSemaphores;
    std::vector<VkFence>            bufferFences;

    void draw() override;
    void createGraphicsPipeline();
    void createCmdPool();
    void createCmdBuffers();
    void recordCmdBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex);
    void createSyncTools();
};