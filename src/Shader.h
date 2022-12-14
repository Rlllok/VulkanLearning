#pragma once

// std
#include <string>
#include <vector>

// vulkan
#include <vulkan/vulkan.h>

class Shader
{
public:

	Shader(VkDevice device, std::string fileName);
	~Shader();

	VkShaderModule getShaderModule();

private:
	VkDevice device;
	std::string fileName;
	std::vector<char> buffer;
	VkShaderModule shaderModule;

	void readFileToBuffer();
	void createShaderModule();
};