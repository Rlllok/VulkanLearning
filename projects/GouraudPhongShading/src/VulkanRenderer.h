#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>	

#include <string>
#include <vector>
#include <optional>

#include "Camera.h"
#include "Model.h"
#include "Texture.h"
#include "Light.h"

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation",
	"VK_LAYER_LUNARG_monitor"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	std::vector<VkPresentModeKHR> presentModes;
};

class VulkanRenderer
{
public:

	VulkanRenderer(bool enableValidationLayers = true);

	void start(int windowWidth, int windowHeight, const char* windowTitle);

private:

	GLFWwindow* window;
	std::string windowTitle;

	Camera* camera;
	
	Model* model = nullptr;
	Texture* texture = nullptr;
	Light* light = nullptr;
	bool gouraudMode = false;
	
	VkResult result = VK_SUCCESS;
	bool enableValidationLayers;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkFramebuffer> swapchainFramebuffer;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	VkPipeline gouraudPipeline;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> bufferFences;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;

	struct {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;

		operator VkDevice() const { return logicalDevice; };

	} device;

	struct {
		std::optional<uint32_t> graphicsQueueIndex;
		std::optional<uint32_t> presentQueueIndex;
		VkQueue graphicsQueue;
		VkQueue presentQueue;

		bool isComplete() {
			return graphicsQueueIndex.has_value()
				&& presentQueueIndex.has_value();
		}
	} queues;

	struct MVP {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
	} mvp;

	VkBuffer mvpBuffer;
	VkDeviceMemory mvpBufferMemory;
	void* mvpBufferMapped;

	VkImage depthImage;
	VkImageView depthImageView;
	VkDeviceMemory depthImageMemory;

	VkImage colorImage;
	VkImageView colorImageView;
	VkDeviceMemory colorImageMemory;

	// Window
	void initWindow(int windowWidth, int windowHeight, const char* windowTitle);
	void loop();
	void processInput(float deltaTime);
	static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
	void fpsCounter(float deltaTime);

	// Vulkan
	void initVulkan();
	void createInstance();
	void setupDebugMessenger();
	void choosePhysicalDevice();
	void createLogicalDevice();
	void createSurface();
	void createSwapchain();
	void createSwapchainImageViews();
	void createColorResources();
	void createRenderPass();
	void createDepthResources();
	void createSwapchainFramebuffers();
	void createDescriptorSetLayout();
	void createDescriptorPool();
	void createDescriptorSet();
	void createGraphicsPipeline();
	void createGouraudPipeline();
	void createCommandPool();
	void createCommandBuffers();
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void createSyncTools();
	void createMVPBuffer();
	void updateMVPBuffer();

	void cleanup();
	void draw();

	void createModel(std::string modelPath, std::string texturePath);

	// Support methods
	std::vector<const char*> getRequiredExtensions();
	bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice);
	void findQueueFamilyIndeces(VkPhysicalDevice physicalDevice);
	bool checkValidationLayerSupport();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
	);
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messengerInfo);
	bool isDeviceSupportExtensions(VkPhysicalDevice physicalDevice);
	SwapchainSupportDetails getSwapchainSupportDetails(VkPhysicalDevice physicalDevice);
	VkSurfaceFormatKHR chooseSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR> availableFormats);
	VkPresentModeKHR chooseSwapchainPresentMode(const std::vector<VkPresentModeKHR> availableModes);
	VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilites);

	// Proxy
	VkResult createDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMesseneger
	);

	void destroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		VkAllocationCallbacks* pAllocator
	);
};