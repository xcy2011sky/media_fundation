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
private:
    GLFWwindow* InitGLWindow(std::string title,int width, int height); 
    GLuint CompileShaderProgram();
    GLuint SetupVAO();

private:
    GLFWwindow* m_pWindow;
    int m_width;
    int m_height;
    std::string m_title;
    GLuint m_shaderProgram;
    GLuint m_vao;
    GLuint m_textureID = 0;
};