#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include "BookTranslatorTests.h"
#include <filesystem>
#include <fstream>

TEST_CASE("searchForOPFFiles works correctly") {
    TestableEpubTranslator translator;

    SECTION("Returns correct path when .opf file exists") {
        std::filesystem::path testDir = "test_dir";
        std::filesystem::create_directory(testDir);
        std::ofstream(testDir / "test.opf").close(); // Create dummy .opf file

        auto result = translator.searchForOPFFiles(testDir);
        REQUIRE(result == testDir / "test.opf");

        std::filesystem::remove_all(testDir); // Cleanup
    }

    SECTION("Returns empty path when no .opf file exists") {
        std::filesystem::path testDir = "test_dir";
        std::filesystem::create_directory(testDir);

        auto result = translator.searchForOPFFiles(testDir);
        REQUIRE(result.empty());

        std::filesystem::remove_all(testDir); // Cleanup
    }
}

TEST_CASE("getSpineOrder works correctly") {
    TestableEpubTranslator translator;

    SECTION("Returns spine order from valid OPF file") {
        std::filesystem::path testOpf = "test.opf";
        std::ofstream file(testOpf);
        file << R"(
            <spine>
                <itemref idref="chapter1" />
                <itemref idref="chapter2" />
            </spine>
        )";
        file.close();

        auto result = translator.getSpineOrder(testOpf);
        REQUIRE(result == std::vector<std::string>{"chapter1.xhtml", "chapter2.xhtml"});

        std::filesystem::remove(testOpf); // Cleanup
    }

    SECTION("Returns empty vector when no <spine> tag exists") {
        std::filesystem::path testOpf = "test.opf";
        std::ofstream file(testOpf);
        file << R"(<package><metadata></metadata></package>)";
        file.close();

        auto result = translator.getSpineOrder(testOpf);
        REQUIRE(result.empty());

        std::filesystem::remove(testOpf); // Cleanup
    }
}

TEST_CASE("getAllXHTMLFiles works correctly") {
    TestableEpubTranslator translator;

    SECTION("Returns all .xhtml files in a directory") {
        std::filesystem::path testDir = "test_dir";
        std::filesystem::create_directory(testDir);
        std::ofstream(testDir / "file1.xhtml").close();
        std::ofstream(testDir / "file2.xhtml").close();
        std::ofstream(testDir / "file.txt").close();

        auto result = translator.getAllXHTMLFiles(testDir);
        REQUIRE(result.size() == 2);
        REQUIRE(std::find(result.begin(), result.end(), testDir / "file1.xhtml") != result.end());
        REQUIRE(std::find(result.begin(), result.end(), testDir / "file2.xhtml") != result.end());

        std::filesystem::remove_all(testDir); // Cleanup
    }

    SECTION("Returns empty vector when no .xhtml files exist") {
        std::filesystem::path testDir = "test_dir";
        std::filesystem::create_directory(testDir);

        auto result = translator.getAllXHTMLFiles(testDir);
        REQUIRE(result.empty());

        std::filesystem::remove_all(testDir); // Cleanup
    }
}

TEST_CASE("sortXHTMLFilesBySpineOrder works correctly") {
    TestableEpubTranslator translator;

    SECTION("Sorts files correctly by spine order") {
        std::vector<std::filesystem::path> xhtmlFiles = {
            "chapter2.xhtml", "chapter1.xhtml", "chapter3.xhtml"
        };
        std::vector<std::string> spineOrder = {"chapter1.xhtml", "chapter2.xhtml", "chapter3.xhtml"};

        auto result = translator.sortXHTMLFilesBySpineOrder(xhtmlFiles, spineOrder);
        REQUIRE(result == std::vector<std::filesystem::path>{"chapter1.xhtml", "chapter2.xhtml", "chapter3.xhtml"});
    }

    SECTION("Handles missing files gracefully") {
        std::vector<std::filesystem::path> xhtmlFiles = {"chapter1.xhtml", "chapter3.xhtml"};
        std::vector<std::string> spineOrder = {"chapter1.xhtml", "chapter2.xhtml", "chapter3.xhtml"};

        auto result = translator.sortXHTMLFilesBySpineOrder(xhtmlFiles, spineOrder);
        REQUIRE(result == std::vector<std::filesystem::path>{"chapter1.xhtml", "chapter3.xhtml"});
    }
}

TEST_CASE("updateContentOpf works correctly") {
    TestableEpubTranslator translator;

    SECTION("Updates OPF file with new spine and manifest") {
        std::filesystem::path testOpf = "test.opf";
        std::ofstream file(testOpf);
        file << R"(
            <package>
                <metadata></metadata>
                <manifest>
                    <item id="oldChapter" href="Text/oldChapter.xhtml" media-type="application/xhtml+xml"/>
                </manifest>
                <spine>
                    <itemref idref="oldChapter" />
                </spine>
            </package>
        )";
        file.close();

        std::vector<std::string> chapters = {"chapter1.xhtml", "chapter2.xhtml"};
        translator.updateContentOpf(chapters, testOpf);

        std::ifstream updatedOpf(testOpf);
        std::string content((std::istreambuf_iterator<char>(updatedOpf)), std::istreambuf_iterator<char>());
        updatedOpf.close();

        REQUIRE(content.find("<item id=\"chapter1\"") != std::string::npos);
        REQUIRE(content.find("<itemref idref=\"chapter1\"") != std::string::npos);
        REQUIRE(content.find("<item id=\"chapter2\"") != std::string::npos);
        REQUIRE(content.find("<itemref idref=\"chapter2\"") != std::string::npos);

        std::filesystem::remove(testOpf); // Cleanup
    }
}

TEST_CASE("cleanChapter works correctly") {
    TestableEpubTranslator translator;

    SECTION("Cleans unwanted tags and spaces") {
        std::filesystem::path testFile = "test.xhtml";
        std::ofstream file(testFile);
        file << R"(
            <html>
                <body>
                    <p>Hello　world!</p>
                    <br />
                    <ruby>Kanji<rt>Reading</rt></ruby>
                </body>
            </html>
        )";
        file.close();

        translator.cleanChapter(testFile);

        std::ifstream cleanedFile(testFile);
        std::string content((std::istreambuf_iterator<char>(cleanedFile)), std::istreambuf_iterator<char>());
        cleanedFile.close();

        REQUIRE(content.find("<br>") == std::string::npos);
        REQUIRE(content.find("<ruby>") == std::string::npos);
        REQUIRE(content.find("\xE3\x80\x80") == std::string::npos);
        REQUIRE(content.find("Hello world!") != std::string::npos);

        std::filesystem::remove(testFile); // Cleanup
    }
}

TEST_CASE("Font Loading", "[GUI]") {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;
    ImFont* font = io.Fonts->AddFontFromFileTTF("../fonts/NotoSansCJKjp-Regular.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    REQUIRE(font != nullptr);
    ImGui::DestroyContext();
}

TEST_CASE("Input Field Updates", "[GUI]") {
    TestableGUI gui;
    strcpy(gui.inputFile, "test.epub");
    strcpy(gui.outputPath, "output/");
    REQUIRE(strcmp(gui.inputFile, "test.epub") == 0);
    REQUIRE(strcmp(gui.outputPath, "output/") == 0);
}


// ----- PDFTranslator -------


TEST_CASE("PDFTranslator: Remove Whitespace") {
    TestablePDFTranslator translator;
    REQUIRE(translator.removeWhitespace("Hello World") == "HelloWorld");
    REQUIRE(translator.removeWhitespace("  a b  c ") == "abc");
    REQUIRE(translator.removeWhitespace("\tNew\nLine\r") == "NewLine");
}

TEST_CASE("PDFTranslator: Extract Text from PDF") {
    TestablePDFTranslator translator;
    
    std::string inputPath = std::filesystem::absolute("../test_files/lorem-ipsum.pdf").string();
    std::string outputPath = std::filesystem::absolute("../test_files/output_text.txt").string();

    // Ensure the test PDF exists
    REQUIRE(std::filesystem::exists(inputPath));

    REQUIRE_NOTHROW(translator.extractTextFromPDF(inputPath, outputPath));
    REQUIRE(std::filesystem::exists(outputPath));

    // Clean up
    std::filesystem::remove(outputPath);
}


TEST_CASE("PDFTranslator: Split Japanese Text") {
    TestablePDFTranslator translator;
    std::string text = "これは長い文です。テストとして分割されるべきです。";
    size_t maxLength = 10;

    std::vector<std::string> sentences = translator.splitJapaneseText(text, maxLength);

    // Print the split sentences
    for (const auto& sentence : sentences) {
        std::cout << "Sentence: " << sentence << std::endl;
    }

    // Ensure there are more than one sentence
    REQUIRE(sentences.size() > 1);
}

TEST_CASE("PDFTranslator: Convert PDF to Images") {
    TestablePDFTranslator translator;
    std::string inputPath = std::filesystem::absolute("../test_files/sample.pdf").string();
    std::string outputDir = std::filesystem::absolute("../test_files/output_images").string();
    float stdDevThreshold = 0;

    // Ensure the test PDF exists
    REQUIRE(std::filesystem::exists(inputPath));

    // Remove existing images directory if it exists
    if (std::filesystem::exists(outputDir)) {
        std::filesystem::remove_all(outputDir);
    }

    // Create the image directory
    std::filesystem::create_directory(outputDir);

    REQUIRE(std::filesystem::exists(outputDir));

    REQUIRE_NOTHROW(translator.convertPdfToImages(inputPath, outputDir, stdDevThreshold));



    // Ensure at least one image file was created
    size_t imageCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(outputDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            ++imageCount;
        }
    }
    REQUIRE(imageCount > 0);

    // Clean up
    std::filesystem::remove_all(outputDir);
}

TEST_CASE("PDFTranslator: Create PDF") {
    TestablePDFTranslator translator;
    std::string inputTextFile = std::filesystem::absolute("./input_text.txt").string();
    std::string imagesDir = std::filesystem::absolute("../test_files").string();
    std::string outputPdf = std::filesystem::absolute("./output.pdf").string();
    // Ensure any pre-existing files are removed
    if (std::filesystem::exists(inputTextFile)) {
        std::filesystem::remove(inputTextFile);
    }
    if (std::filesystem::exists(outputPdf)) {
        std::filesystem::remove(outputPdf);
    }

    // Create test input text file
    std::ofstream inputFile(inputTextFile);
    REQUIRE(inputFile.is_open());
    inputFile << "1,This is a test sentence." << std::endl;
    inputFile << "2,Another sentence to test wrapping." << std::endl;
    inputFile.close();

    // Run PDF creation
    REQUIRE_NOTHROW(translator.createPDF(outputPdf, inputTextFile, imagesDir));

    // Ensure output PDF is created
    REQUIRE(std::filesystem::exists(outputPdf));

    // Clean up
    REQUIRE(std::filesystem::remove(inputTextFile));
    REQUIRE(std::filesystem::remove(outputPdf));
}
