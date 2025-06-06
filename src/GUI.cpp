#include "GUI.h"
#include "TranslatorFactory.h"

void GUI::init(GLFWwindow *window, const char *glsl_version) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Load the Japanese font as primary
    ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/NotoSansCJKjp-Regular.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    if (font == nullptr) {
        std::cout << "Failed to load font!" << std::endl;
    }
    ImFontConfig defaultFontConfig;
    defaultFontConfig.SizePixels = 22.0f;
    defaultFontConfig.MergeMode = true;
    // Merge the default font to provide missing glyphs (e.g., double quotes)
    io.Fonts->AddFontDefault(&defaultFontConfig);

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

    populateLanguages();

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

    populateLanguages();

    if (localModelStr == "Application's Translator") {
        ImGui::Text("Source Language:");
        ImGui::Combo("##source_language", &selectedLanguageIndex, languageNamesCStr.data(), languageNamesCStr.size());

        std::string selectedLanguage = languageNames[selectedLanguageIndex];
        sourceLanguageCode = language_code_map.at(selectedLanguage);
    }
    

    // Input fields for directories
    ImGui::InputText("Original Book", inputFile, sizeof(inputFile));
    // Browse button for the epub to convert
    ImGui::PushID("epub_browse_button"); // Unique ID for the first button
    if (ImGui::Button("Browse")) {
        nfdchar_t* outPath = nullptr;
        // Set up filter for EPUB files
        nfdfilteritem_t filterItem[1] = { { "EPUB/PDF/DOCX/HTML Files", "epub,pdf,docx,html" } };
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

    // add a button to edit book details if the inputFile has an epub file extension
    std::string inputFileStr(inputFile);
    if (inputFileStr.substr(inputFileStr.find_last_of(".") + 1) == "epub") {
        renderEditBookPopup();
    }

    
    if (inputFileStr.substr(inputFileStr.find_last_of(".") + 1) == "html" && localModel == 1) {
        showWarning = true; // We want to show the modal
        localModel = 0; // Force fallback to "0" translator
    }

    // Show warning modal if needed
    if (showWarning) {
        ImGui::Text("Warning: DeepL Translator is not supported for HTML files.");
    }

    
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
        showWarning = false; // Reset the warning flag

        // Ensure previous thread is joined before starting a new one
        if (workerThread.joinable()) {
            workerThread.join(); // Wait for previous thread to complete before launching a new one
        }

        // Check if the output path exists
        std::filesystem::path outputPathStr = std::filesystem::u8path(outputPath);
        if (!std::filesystem::exists(outputPathStr)) {
            std::cout << "Output path does not exist: " << outputPathStr.u8string() << std::endl;
            invalidInput = true;
        }

        // Check if the input file exists
        std::filesystem::path inputFilePath = std::filesystem::u8path(inputFile);
        if (!std::filesystem::exists(inputFilePath)) {
            std::cout << "Input file does not exist: " << inputFilePath.u8string() << std::endl;
            invalidInput = true;
        }

        // Check if the input file is a valid EPUB/PDF/DOCX/HTML file
        std::string fileExtension = inputFilePath.extension().string();
        if (!fileExtension.empty() && fileExtension[0] == '.')
            fileExtension = fileExtension.substr(1); // remove the leading '.'

        std::vector<std::string> supportedFiles = { "epub", "pdf", "docx", "html" };

        // Check if the output path is a directory
        if (!std::filesystem::is_directory(outputPathStr)) {
            std::cout << "Output path is not a directory: " << outputPathStr.u8string() << std::endl;
            invalidInput = true;
        }

        // Check if the input file is a directory
        if (std::filesystem::is_directory(inputFilePath)) {
            std::cout << "Input file is a directory: " << inputFilePath.u8string() << std::endl;
            invalidInput = true;
        } else if (std::find(supportedFiles.begin(), supportedFiles.end(), fileExtension) == supportedFiles.end()) {
            std::cout << "Unsupported file type: " << fileExtension << std::endl;
            invalidInput = true;
        }

        if (invalidInput) {
            std::cout << "Please check your inputs and retry." << std::endl;
            running = false;   // Reset the running flag
            finished = true;   // Mark as finished
            invalidInput = false; // Reset the invalid input flag
            return;
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
                        // Write book details to a text file
                        std::ofstream bookDetails("book_details.txt");
                        if (bookDetails.is_open()) {
                            bookDetails << bookTitle << "\n" << bookAuthor;
                            bookDetails.close();
                        } else {
                            throw std::runtime_error("Failed to open book details file for writing");
                        }
                    } else if (fileExtension == "pdf") {
                        translator = TranslatorFactory::createTranslator("pdf");
                    } else if (fileExtension == "docx") {
                        translator = TranslatorFactory::createTranslator("docx");
                    } else if (fileExtension == "html") {
                        translator = TranslatorFactory::createTranslator("html");


                    } else {
                        throw std::runtime_error("Unsupported file type: " + fileExtension);
                    }

                    // Run the translator
                    result = translator->run(inputFile, outputPath, localModel, deepLKey, sourceLanguageCode);
                    
                    if (std::filesystem::exists("book_details.txt")) {
                        std::filesystem::remove("book_details.txt");
                    }
                    

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

    if (running) {
        ImGui::Text("Translation in progress...");
        ImGui::SameLine();
        ShowSpinner();
        ImGui::NewLine();
    }

    if (finished) {
        std::lock_guard<std::mutex> lock(resultMutex);
        ImGui::Text("%s", statusMessage.c_str());
    }

    // Log window
    float logHeight = ImGui::GetContentRegionAvail().y - LOG_WINDOW_PADDING;

    ImGui::Text("Logs:");
    ImGui::BeginChild("LogChild", ImVec2(0, logHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushTextWrapPos();

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
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);


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
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.85f, 0.90f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.85f, 0.85f, 0.90f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.85f, 0.85f, 0.90f, 1.0f);
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

void GUI::ShowSpinner(float radius, int numSegments, float thickness) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += radius;
    center.y += radius;

    float start = (float)ImGui::GetTime() * 2.0f;
    for (int i = 0; i < numSegments; i++) {
        float angle = start + (i * IM_PI * 2.0f / numSegments);
        ImVec2 startPos = ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius);
        ImVec2 endPos = ImVec2(center.x + cos(angle) * (radius - thickness), center.y + sin(angle) * (radius - thickness));

        float alpha = (float)i / (float)numSegments;
        // Change color based o
        ImU32 color;
        if (isDarkTheme){
           color = ImColor(1.0f, 1.0f, 1.0f, alpha);
        } else {
            color = ImColor(0.0f, 0.0f, 0.0f, alpha);
        }

        drawList->AddLine(startPos, endPos, color, thickness);
    }
}

void GUI::renderEditBookPopup() {


    // Show the button
    if (ImGui::Button("Edit Book")) {
        showPopup = true;
        strncpy(titleBuffer, bookTitle.c_str(), sizeof(titleBuffer) - 1);
        strncpy(authorBuffer, bookAuthor.c_str(), sizeof(authorBuffer) - 1);
    }

    // Open the pop-up
    if (showPopup) {
        ImGui::OpenPopup("Edit Book Details");
    }

    // Pop-up logic
    if (ImGui::BeginPopupModal("Edit Book Details", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Title", titleBuffer, sizeof(titleBuffer));
        ImGui::InputText("Author", authorBuffer, sizeof(authorBuffer));

        if (ImGui::Button("Done")) {
            bookTitle = titleBuffer;
            bookAuthor = authorBuffer;
            showPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            showPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void GUI::populateLanguages() {
    if (!languageNames.empty()) return;

    // Extract language names from the unordered_map
    for (const auto& entry : language_code_map) {
        languageNames.push_back(entry.first);
    }

    // Sort alphabetically
    std::sort(languageNames.begin(), languageNames.end());

    // Convert to const char* format for ImGui
    languageNamesCStr.clear();
    for (const auto& name : languageNames) {
        languageNamesCStr.push_back(name.c_str());
    }
}