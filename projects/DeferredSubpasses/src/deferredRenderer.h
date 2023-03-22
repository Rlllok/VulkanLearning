#pragma once

#include "volk.h"
#include <glm/glm.hpp>

#include "baseRenderer.h"
#include "camera.h"
#include "model.h"
#include "texture.h"
#include "light.h"

#include <vector>

class DeferredRenderer : public BaseRenderer
{
public:
    DeferredRenderer(bool isDebug);
    ~DeferredRenderer();

    void prepareRenderer() override;
    
private:
    Camera*     camera = nullptr;
    Model*      model = nullptr;
    Texture*    texture = nullptr;
    Light*      light = nullptr;

    VkPipelineLayout    gBufferPipelineLayout = VK_NULL_HANDLE;
    VkPipeline          gBufferPipeline = VK_NULL_HANDLE;
    VkPipelineLayout    composePipelineLayout = VK_NULL_HANDLE;
    VkPipeline          composePipeline = VK_NULL_HANDLE;

    VkCommandPool                   cmdPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>    cmdBuffers;

    std::vector<VkSemaphore>    imageAvailableSemaphores;
    std::vector<VkSemaphore>    renderFinishedSemaphores;
    std::vector<VkFence>        bufferFences;

    VkDescriptorPool                inputDescriptorPool;
    VkDescriptorSetLayout           inputDescriptorSetLayout;
    std::vector<VkDescriptorSet>    inputDescriptorSets;

    VkDescriptorPool                lightDescriptorPool;
    VkDescriptorSetLayout           lightDescriptorSetLayout;
    std::vector<VkDescriptorSet>    lightDescriptorSets;

    struct MVP {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    } mvp;

    VkDescriptorPool        descriptorPool;
    VkDescriptorSetLayout   descriptorSetLayout;
    VkDescriptorSet         descriptorSet;

    VkBuffer        mvpBuffer;
    VkDeviceMemory  mvpBufferMemory;
    void*           mvpBufferMapped;

    struct PushConstant {
        int mode = 0;
    } pushConstant;

    VkImage         depthImage;
    VkImageView     depthImageView;
    VkDeviceMemory  depthImageMemory;

    std::vector<VkImage>        colorImages;
    std::vector<VkImageView>    colorImageViews;
    std::vector<VkDeviceMemory> colorImageMemories;

    std::vector<VkImage>        normImages;
    std::vector<VkImageView>    normImageViews;
    std::vector<VkDeviceMemory> normImageMemories;

    std::vector<VkImage>        positionImages;
    std::vector<VkImageView>    positionImageViews;
    std::vector<VkDeviceMemory> positionImageMemories;

    void draw() override;

    void createRenderPass() override;
    void createColorAttachments();
    void createNormAttachments();
    void createPositionAttachments();
    void createDepthResources();
    void createSwapchainFramebuffers() override;
    void createDescriptorSetLayout();
    void createInputDescriptorSetLayout();
    void createLightDescriptorSetLayout();
    void createDescriptorPool();
    void createInputDescriptorPool();
    void createLightDescriptorPool();
    void createDescriptorSet();
	void createInputDescriptorSet();
	void createLightDescriptorSets();
    void createGbufferPipeline();
    void createComposePipeline();
    void createCmdPool();
    void createCmdBuffers();
    void recordCmdBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex);
    void createSyncTools();
    void createMVPBuffer();
    void updateMVPBuffer();
};