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
    ImGui::StyleColorsLight();

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
        nfdfilteritem_t filterItem[1] = { { "EPUB/PDF Files", "epub,pdf" } };
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
    ImGui::Text("Logs:");
    ImGui::BeginChild("LogChild", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);
    std::string logContents = logStream.str();
    ImGui::TextUnformatted(logContents.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f); // Auto-scroll to bottom
    }
    ImGui::EndChild();

    // // Test button
    // if (ImGui::Button("PDF Test")) {
    //     // Call PDFTranslator run
    //     std::cout << "Calling PDFTranslator run" << std::endl;
    //     PDFTranslator parser;
    //     parser.run();
    // }

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
