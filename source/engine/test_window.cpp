#include "test_window.h"

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
    return true;
}

void update_window()
{
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }
}

void terminate_window()
{
    glfwTerminate();
}