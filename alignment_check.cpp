#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1, 1, "", NULL, NULL);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glewInit();
    GLint alignment;
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &alignment);
    std::cout << alignment << std::endl;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
