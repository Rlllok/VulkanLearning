#pragma once

#include "baseRenderer.h"
#include "camera.h"
#include "model.h"
#include "texture.h"
#include "light.h"

class PhongGouraudRenderer : public BaseRenderer
{
public:
    PhongGouraudRenderer(bool isDebug);
    ~PhongGouraudRenderer();

    void prepareRenderer() override;
    void draw() override;

private:
    Camera*     camera;
    Light*      light;
    Model*      model;
    Texture*    texture;

    bool gouraudMode = false;

    struct MVP {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
	} mvp;

    VkBuffer        mvpBuffer;
    VkDeviceMemory  mvpBufferMemory;
    void*           mvpBufferMapped;

    VkImage         colorImage;
    VkImageView     colorImageView;
    VkDeviceMemory  colorImageMemory;

    VkImage         depthImage;
    VkImageView     depthImageView;
    VkDeviceMemory  depthImageMemory;

    VkDescriptorSetLayout   descriptorSetLayout;
    VkDescriptorPool        descriptorPool;
    VkDescriptorSet         descriptorSet;

    VkPipelineLayout    phongPipelineLayout;
    VkPipeline          phongPipeline;
    VkPipelineLayout    gouraudPipelineLayout;
    VkPipeline          gouraudPipeline;

    VkCommandPool                   cmdPool;
    std::vector<VkCommandBuffer>    cmdBuffers;

    std::vector<VkSemaphore>    imageAvailableSemaphores;
    std::vector<VkSemaphore>    renderFinishedSemaphores;
    std::vector<VkFence>        bufferFences;

    void createColorResources();
    void createDepthResources();
    void createRenderPass() override;
    void createSwapchainFramebuffers() override;
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSet();
    void createPipelines();
    void createCmdPool();
    void createCmdBuffers();
    void recordCmdBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex);
    void createSyncTools();
    void createMVPBuffer();
    void updateMVPBuffer();
};