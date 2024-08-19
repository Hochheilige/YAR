#include "imgui_layer.h"

#include <GLFW/glfw3.h>

GLFWwindow* window;

bool init_window()
{
    if (!glfwInit())
        return false;

    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);

    imgui_init(window);

    return true;
}

void update_window()
{
    while (!glfwWindowShouldClose(window))
    {
        imgui_begin_frame();

        glClearColor(gBackGroundColor[0], gBackGroundColor[1], gBackGroundColor[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        imgui_render();
        imgui_end_frame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void terminate_window()
{
    imgui_terminate();
    glfwTerminate();
}