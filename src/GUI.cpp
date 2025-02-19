#include "GUI.h"
#include "TranslatorFactory.h"

void GUI::init(GLFWwindow *window, const char *glsl_version) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Load a font that supports Japanese characters
    ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/NotoSansCJKjp-Regular.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    if (font == nullptr) {
        std::cout << "Failed to load font!" << std::endl;
    }

    // Rebuild the font atlas after adding the new font
    io.Fonts->Build();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Check theme file exists if it doesnt default to light theme
    if (!std::filesystem::exists(themeFile)) {
        isDarkTheme = false;

        // Create theme file put light theme in it
        std::ofstream theme(themeFile);

        if (theme.is_open()) {
            theme << "light";
            theme.close();
            setCustomLightStyle();
        } else {
            std::cerr << "Failed to open theme file for writing" << std::endl;
        }

    } else {
        std::ifstream theme(themeFile);
        if (theme.is_open()) {
            std::string themeStr;
            theme >> themeStr;
            if (themeStr == "dark") {
                isDarkTheme = true;
                setCustomDarkStyle();
            } else {
                isDarkTheme = false;
                setCustomLightStyle();
            }
        } else {
            std::cerr << "Failed to open theme file for reading" << std::endl;
        }
    }

    running = false;   // Initialize flags
    finished = false;
}

void GUI::render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUI::update(std::ostringstream& logStream) {
    // The GUI code
    ImGui::Begin("Epub Translator", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

    // Render the menu bar
    renderMenuBar();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20); // Push content down

    // Begin Combo box for selecting the local model
    ImGui::Text("Select Translator:");
    const char* items[] = { "Application's Translator", "DeepL Translator" };
    ImGui::Combo("##combo", &localModel, items, IM_ARRAYSIZE(items));

    std::string localModelStr = items[localModel];

    if (localModelStr == "DeepL Translator") {
        ImGui::Text("Note: DeepL Translator is higher quality but requires a DeepL API key");
        ImGui::InputText("DeepL API Key", deepLKey, sizeof(deepLKey));
    }

    // Input fields for directories
    ImGui::InputText("Original Book", inputFile, sizeof(inputFile));
    // Browse button for the epub to convert
    ImGui::PushID("epub_browse_button"); // Unique ID for the first button
    if (ImGui::Button("Browse")) {
        nfdchar_t* outPath = nullptr;
        // Set up filter for EPUB files
        nfdfilteritem_t filterItem[1] = { { "EPUB/PDF/DOCX Files", "epub,pdf,docx" } };
        nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, 1, NULL);
        if (result == NFD_OKAY) {
            strcpy(inputFile, outPath);
            free(outPath);
        }
    }
    ImGui::PopID(); // End unique ID for the first button

    // Output path input
    ImGui::InputText("Translated Output Location", outputPath, sizeof(outputPath));

    // Output folder browse button
    ImGui::PushID("output_browse_button"); // Unique ID for the second button
    if (ImGui::Button("Browse")) {
        nfdchar_t* outPath = nullptr;
        nfdresult_t result = NFD_PickFolder(&outPath, NULL);
        if (result == NFD_OKAY) {
            strcpy(outputPath, outPath);
            free(outPath);
        }
    }
    ImGui::PopID(); // End unique ID for the second button

    // Check if both fields are filled
    bool enableButton = strlen(inputFile) > 0 && strlen(outputPath) > 0;

    // Disable button if fields are empty
    if (!enableButton || running) {
        ImGui::BeginDisabled();
    }

    // Button to trigger the run function
    if (ImGui::Button("Start Translation") && enableButton && !running) {
        running = true;      // Set the running flag
        finished = false;    // Reset the finished flag

        // Ensure previous thread is joined before starting a new one
        if (workerThread.joinable()) {
            workerThread.join(); // Wait for previous thread to complete before launching a new one
        }

        // Start the worker thread
        workerThread = std::thread([this, &logStream]() {
            {
                std::lock_guard<std::mutex> lock(resultMutex); // Protect shared resources
                std::string inputFileStr(inputFile);
                std::string outputPathStr(outputPath);

                logStream << "Starting Translation...\n"; // Log start message

                // Determine file extension
                std::string fileExtension = inputFileStr.substr(inputFileStr.find_last_of(".") + 1);
                std::unique_ptr<Translator> translator;

                try {
                    if (fileExtension == "epub") {
                        translator = TranslatorFactory::createTranslator("epub");
                    } else if (fileExtension == "pdf") {
                        translator = TranslatorFactory::createTranslator("pdf");
                    } else if (fileExtension == "docx") {
                        translator = TranslatorFactory::createTranslator("docx");
                    } else {
                        throw std::runtime_error("Unsupported file type: " + fileExtension);
                    }

                    // Run the translator
                    result = translator->run(inputFile, outputPath, localModel, deepLKey);
                } catch (const std::exception& e) {
                    logStream << "Error: " << e.what() << "\n";
                    result = -1;
                }

            }

            finished = true;  // Mark as finished
            running = false;  // Reset the running flag

            if (result == 0) {
                statusMessage = "Translation successful!";
                logStream << "Translation completed successfully.\n"; // Log success
            } else {
                statusMessage = "Translation failed!";
                logStream << "Translation failed with error code: " << result << "\n"; // Log failure
            }
        });
    }

    if (!enableButton || running) {
        ImGui::EndDisabled();
    }

    if (finished) {
        std::lock_guard<std::mutex> lock(resultMutex);
        ImGui::Text("%s", statusMessage.c_str());
    }

    // Log window
    float logHeight = ImGui::GetContentRegionAvail().y - LOG_WINDOW_PADDING;

    ImGui::Text("Logs:");
    ImGui::BeginChild("LogChild", ImVec2(0, logHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

    std::string logContents = logStream.str();
    ImGui::TextUnformatted(logContents.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f); // Auto-scroll to bottom
    }
    ImGui::EndChild();

    ImGui::End();
}

void GUI::shutdown() {
    if (workerThread.joinable()) {
        workerThread.join(); // Wait for the worker thread to finish
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void GUI::newFrame() {
    // feed inputs to dear imgui, start new frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
}

void GUI::setCustomDarkStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.38f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.45f, 0.45f, 0.48f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.23f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.7f, 0.5f, 0.9f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.38f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.23f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.33f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.35f, 0.38f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.32f, 1.0f);
    colors[ImGuiCol_Separator] = ImVec4(0.35f, 0.35f, 0.37f, 1.0f);
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
}


void GUI::setCustomLightStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.98f, 1.0f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.85f, 0.85f, 0.90f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.85f, 0.85f, 0.90f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.80f, 0.80f, 0.85f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.75f, 0.75f, 0.80f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.5f, 0.7f, 1.0f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.70f, 0.75f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.90f, 0.90f, 0.93f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.80f, 0.80f, 0.85f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.95f, 0.95f, 0.98f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
    colors[ImGuiCol_Separator] = ImVec4(0.80f, 0.80f, 0.83f, 1.0f);
    colors[ImGuiCol_Text] = ImVec4(0.10f, 0.10f, 0.15f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
}


void GUI::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        // Push "Settings" to the right
        float width = ImGui::GetContentRegionAvail().x; // Get available width
        ImGui::SetCursorPosX(width - 75); // Move cursor position near the right

        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Light Theme", nullptr, !isDarkTheme)) {
                isDarkTheme = false;
                setCustomLightStyle();

                // Open themeFile
                std::ofstream theme(themeFile);

                if (theme.is_open()) {
                    theme << "light";
                    theme.close();
                } else {
                    std::cerr << "Failed to open theme file for writing" << std::endl;
                }

                theme.close();
            }
            if (ImGui::MenuItem("Dark Theme", nullptr, isDarkTheme)) {
                isDarkTheme = true;
                setCustomDarkStyle();

                // Open themeFile
                std::ofstream theme(themeFile);

                if (theme.is_open()) {
                    theme << "dark";
                    theme.close();
                } else {
                    std::cerr << "Failed to open theme file for writing" << std::endl;
                }

                theme.close();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}


void GUI::handleFileDrop(int count, const char** paths) {
    // If count count is more than 1 print drag only one file message
    if (count > 1) {
        std::cout << "Please only drag one file at a time." << std::endl;
        return;
    }


    if (count == 1) {
        ImGui::ClearActiveID();

        strcpy(inputFile, paths[0]);
    }
}