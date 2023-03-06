#pragma once

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#include "android_native_app_glue.h"
#endif

// std
#include <string>
#include <vector>

// vulkan
#include "volk.h"

class Shader
{
public:

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	Shader(VkDevice device, std::string fileName, struct android_app* app);
#else
	Shader(VkDevice device, std::string fileName);
#endif
	~Shader();

	VkShaderModule getShaderModule();

private:
	VkDevice device;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	struct android_app* app;
#endif
	std::string fileName;
	std::vector<char> buffer;
	VkShaderModule shaderModule;

	void readFileToBuffer();
	void createShaderModule();
};