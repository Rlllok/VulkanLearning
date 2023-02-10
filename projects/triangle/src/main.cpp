#include "triangleRenderer.h"

// std
#include <iostream>

int main()
{
	const int windowWidth = 800;
	const int windowHeight = 600;

	TriangleRenderer renderer(true);

	try {
		renderer.start(windowWidth, windowHeight, "Triangle");
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}