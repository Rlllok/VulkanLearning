#include "deferredRenderer.h"

#include <iostream>

int main()
{
	const int windowWidth = 800;
	const int windowHeight = 600;

	DeferredRenderer renderer(true);

	try {
		renderer.initWindow(windowWidth, windowHeight, "DeferredSubpasses");
		renderer.initVulkan();
		renderer.prepareRenderer();
		renderer.startRenderLoop();
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}