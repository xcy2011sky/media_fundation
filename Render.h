#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>

class Render {
public:
    Render(std::string title,int width, int height);
    ~Render() = default;
    
    void updateRGBTexture(int width, int height, const unsigned char* rgbData);
    void RenderFrame();
    bool isWindowsClose() const { return glfwWindowShouldClose(m_pWindow); }
private:
    GLFWwindow* InitGLWindow(std::string title,int width, int height); 
    GLuint CompileShaderProgram();
    GLuint SetupVAO();

private:
    GLFWwindow* m_pWindow = nullptr;
    int m_width = 0;
    int m_height = 0;
    std::string m_title = "";
    GLuint m_shaderProgram = 0;
    GLuint m_vao = 0;
    GLuint m_textureID = 0;
};