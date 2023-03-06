#include "triangleRenderer.h"

// std
#include <iostream>

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void handleAppCmd(struct android_app* app, int32_t cmd) {
	auto* renderer = reinterpret_cast<TriangleRenderer*>(app->userData);

	switch (cmd) {
		case APP_CMD_INIT_WINDOW:
			if (app->window != nullptr) {
				renderer->initWindow(app->window);
				renderer->initVulkan();
			}
			break;
		case APP_CMD_TERM_WINDOW:
			break;
		default:
			break;
	}
}

void android_main(struct android_app* app)
{
	auto triangleRenderer = new TriangleRenderer(app, true);
	app->userData = triangleRenderer;
	app->onAppCmd = handleAppCmd;
	triangleRenderer->startLoop(app);
	
	delete triangleRenderer;
}
#else
int main()
{
	const int windowWidth = 800;
	const int windowHeight = 600;

	TriangleRenderer renderer(true);

	try {
		renderer.initWindow(windowWidth, windowHeight, "Triangle");
		renderer.initVulkan();
		renderer.startLoop();
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
#endif