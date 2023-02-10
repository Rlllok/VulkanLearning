#pragma once

#include <GLFW/glfw3.h>

#include <string>
#include <vector>

class Window
{
public:

    Window(uint32_t width, uint32_t height, std::string title);
    ~Window();
    static std::vector<const char*> getRequiredExtensions();

    void runLoop();

    GLFWwindow* getWindow() { return window; }

private:

    GLFWwindow* window;

    uint32_t width;
    uint32_t height;
    std::string title;

    void init();
    void loop();

};