#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <nfd.h>

class GUI {
public:
    void init(GLFWwindow *window, const char *glsl_version);
    void render();
    void update();
    void shutdown();
    void newFrame();
private:
    char epubToConvert[256] = "";
    char outputPath[256] = "";
    int result = 2;
};