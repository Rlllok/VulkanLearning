#include "VulkanRenderer.h"

// std
#include <iostream>

int main()
{
	const int windowWidth = 800;
	const int windowHeight = 600;
	const bool enableValidationLayers = true;

	VulkanRenderer renderer(enableValidationLayers);

	try {
		renderer.start(windowWidth, windowHeight, "Vulkan Learning");
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	

	return EXIT_SUCCESS;
}