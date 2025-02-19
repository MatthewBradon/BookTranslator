#pragma once

#define LOG_WINDOW_PADDING 22.0f

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <nfd.h>
#include <thread>
#include <atomic>
#include <mutex>
#include "imgui_internal.h"

class GUI {
public:
    void init(GLFWwindow *window, const char *glsl_version);
    void render();
    void update(std::ostringstream& logStream);
    void shutdown();
    void newFrame();
    void handleFileDrop(int count, const char** paths);
protected:
    char inputFile[256] = "";
    char outputPath[256] = "";
    int localModel = 0;
    char deepLKey[256] = "";  // Ensure it is zero-initialized
    std::thread workerThread;
    std::atomic<bool> running;
    std::atomic<bool> finished;
    std::mutex resultMutex;
    std::string statusMessage;
    int result = -1;
    bool isDarkTheme = true;
    std::string themeFile = "theme.txt";
    void setCustomDarkStyle();
    void setCustomLightStyle();
    void renderMenuBar();
    void ShowSpinner(float radius = 10.0f, int numSegments = 12, float thickness = 2.0f);
};