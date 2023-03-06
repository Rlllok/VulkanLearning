#include "shader.h"

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#include <android/asset_manager.h>
#endif

// std
#include <fstream>

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
Shader::Shader(VkDevice device, std::string fileName, struct android_app* app)
{
	this->device = device;
	this->fileName = fileName;
	this->app = app;

	readFileToBuffer();
	createShaderModule();
}
#else
Shader::Shader(VkDevice device, std::string fileName)
{
	this->device = device;
	this->fileName = fileName;

	readFileToBuffer();
	createShaderModule();
}
#endif

Shader::~Shader()
{
	vkDestroyShaderModule(device, shaderModule, nullptr);
}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void Shader::readFileToBuffer()
{
	AAsset* asset = AAssetManager_open(app->activity->assetManager, fileName.c_str(), AASSET_MODE_STREAMING);
	size_t fileSize = AAsset_getLength(asset);

	buffer.resize(fileSize);
	AAsset_read(asset, buffer.data(), fileSize);
	AAsset_close(asset);
}
#else
void Shader::readFileToBuffer()
{
	/*
		Open file as binary. std::ios::ate means open at the end of the file.
		We have to open file at the end to get the lengs of the file in terms of chars.
	*/
	std::ifstream file(fileName, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("ERROR: cannot open \"" + fileName + "\".");
	}

	size_t fileSize = (size_t)file.tellg();
	buffer.resize(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
}
#endif

void Shader::createShaderModule()
{
	VkShaderModuleCreateInfo shaderModuleInfo = {};
	shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleInfo.codeSize = static_cast<uint32_t>(buffer.size());
	shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

	VkResult result = vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("ERROR: cannot create Shader Module from file \"" + fileName + "\".");
	}
}

VkShaderModule Shader::getShaderModule()
{
	return shaderModule;
}
