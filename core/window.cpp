#include "window.h"
#include "errorLog.h"

Window::Window(uint32_t width, uint32_t height, std::string title)
{
    this->width = width;
    this->height = height;
    this->title = title;

    init();
}
Window::~Window()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

std::vector<const char*> Window::getRequiredExtensions()
{
	uint32_t extensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);

	return extensions;
}

void Window::runLoop()
{
    loop();
}

void Window::init()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    if (!window) {
        error::log("cannot create GLFW window.");
    }

    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Window::loop()
{
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }
}
