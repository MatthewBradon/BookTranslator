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
        REQUIRE(content.find("\u3000") == std::string::npos);
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
