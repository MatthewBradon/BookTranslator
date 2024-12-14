#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <nfd.h>
#include <thread>
#include <atomic>
#include <mutex>

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

    std::thread workerThread;    // Thread for the run function
    std::atomic<bool> running;  // Flag to indicate if the worker is running
    std::atomic<bool> finished; // Flag to indicate if the worker is finished
    std::mutex resultMutex;     // Mutex to protect shared resources
    std::string statusMessage;  // Shared status message
    int result = -1;            // Result of the conversion
};