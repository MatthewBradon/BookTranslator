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
#include "langcodes.h"

class GUI {
public:
    void init(GLFWwindow *window, const char *glsl_version);
    void render();
    void update(std::ostringstream& logStream);
    void shutdown();
    void newFrame();
    void handleFileDrop(int count, const char** paths);
protected:
    char inputFile[1024] = "";
    char outputPath[1024] = "";
    char titleBuffer[1024] = "";
    char authorBuffer[1024] = "";
    bool showPopup = false;
    std::string bookTitle = "";
    std::string bookAuthor = "";
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
    int selectedLanguageIndex = 0;
    std::vector<std::string> languageNames;
    std::vector<const char*> languageNamesCStr;
    std::string sourceLanguageCode;
    bool showWarning = false;
    bool invalidInput = false;
    void setCustomDarkStyle();
    void setCustomLightStyle();
    void renderMenuBar();
    void ShowSpinner(float radius = 10.0f, int numSegments = 12, float thickness = 2.0f);
    void renderEditBookPopup();
    void populateLanguages();
};