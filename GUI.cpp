#include "GUI.h"
#include "main.h"

void GUI::init(GLFWwindow *window, const char *glsl_version){
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
        ImGui::StyleColorsDark();
    }

void GUI::render(){
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUI::update(){
        // The GUI code
        ImGui::Begin("Epub Translator", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

                // Input fields for directories
        ImGui::InputText("Epub To Convert", epubToConvert, sizeof(epubToConvert));
        ImGui::InputText("Output Path", outputPath, sizeof(outputPath));

        // Check if both fields are filled
        bool enableButton = strlen(epubToConvert) > 0 && strlen(outputPath) > 0;

        // Disable button if fields are empty
        if (!enableButton) {
            ImGui::BeginDisabled();
        }
        
        // Button to trigger the run function
        if (ImGui::Button("Run Conversion") && enableButton) {
            result = 2;
            std::string epubToConvertStr(epubToConvert);
            std::string outputPathStr(outputPath);
            result = run(epubToConvertStr, outputPathStr);
        }
 
        if (!enableButton) {
            ImGui::EndDisabled();
        }

        // Display the result of the conversion
        if (result == 0) {
            ImGui::Text("Translation successful!");
        } else if (result == 1) {
            ImGui::Text("Translation failed!");
        }

        ImGui::End();
    }

void GUI::shutdown(){
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
void GUI::newFrame(){
    // feed inputs to dear imgui, start new frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
}


