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

TEST_CASE("extractSpineContent works correctly") {
    TestableEpubTranslator translator;

    SECTION("Extracts content from a valid <spine> tag") {
        std::string content = R"(
            <package>
                <spine toc="ncx">
                    <itemref idref="chapter1" />
                    <itemref idref="chapter2" />
                </spine>
            </package>
        )";

        std::string expected = R"(
                    <itemref idref="chapter1" />
                    <itemref idref="chapter2" />
                )";

        REQUIRE(translator.extractSpineContent(content) == expected);
    }

    SECTION("Handles <spine> with no itemrefs") {
        std::string content = R"(<spine></spine>)";
        REQUIRE(translator.extractSpineContent(content).empty());
    }

    SECTION("Throws error when <spine> tag is missing") {
        std::string content = R"(<package><metadata></metadata></package>)";

        try {
            translator.extractSpineContent(content);
            FAIL("Expected std::runtime_error not thrown");
        } catch (const std::runtime_error& e) {
            REQUIRE(std::string(e.what()) == "No <spine> tag found in the OPF file.");
        } catch (...) {
            FAIL("Unexpected exception type thrown");
        }
    }

    SECTION("Handles attributes inside the <spine> tag") {
        std::string content = R"(<spine toc="ncx" linear="yes"><itemref idref="chapter1" /></spine>)";
        std::string expected = R"(<itemref idref="chapter1" />)";
        REQUIRE(translator.extractSpineContent(content) == expected);
    }

    SECTION("Handles newline characters in <spine> content") {
        std::string content = R"(
            <spine>
                <itemref idref="chapter1" />
                <itemref idref="chapter2" />
            </spine>
        )";

        std::string expected = R"(
                <itemref idref="chapter1" />
                <itemref idref="chapter2" />
            )";

        REQUIRE(translator.extractSpineContent(content) == expected);
    }
}

TEST_CASE("extractIdrefs works correctly") {
    TestableEpubTranslator translator;

    SECTION("Extracts idrefs from valid spine content") {
        std::string spineContent = R"(
            <itemref idref="chapter1" />
            <itemref idref="chapter2" />
        )";

        std::vector<std::string> expected = {"chapter1.xhtml", "chapter2.xhtml"};
        REQUIRE(translator.extractIdrefs(spineContent) == expected);
    }

    SECTION("Handles idrefs that already have .xhtml extension") {
        std::string spineContent = R"(
            <itemref idref="chapter1.xhtml" />
            <itemref idref="chapter2.xhtml" />
        )";

        std::vector<std::string> expected = {"chapter1.xhtml", "chapter2.xhtml"};
        REQUIRE(translator.extractIdrefs(spineContent) == expected);
    }

    SECTION("Returns empty vector when no idref attributes are present") {
        std::string spineContent = R"(<itemref />)";
        REQUIRE(translator.extractIdrefs(spineContent).empty());
    }

    SECTION("Handles mixed content with and without .xhtml extensions") {
        std::string spineContent = R"(
            <itemref idref="chapter1" />
            <itemref idref="chapter2.xhtml" />
        )";

        std::vector<std::string> expected = {"chapter1.xhtml", "chapter2.xhtml"};
        REQUIRE(translator.extractIdrefs(spineContent) == expected);
    }

    SECTION("Ignores malformed idref attributes") {
        std::string spineContent = R"(
            <itemref idref="chapter1" />
            <itemref idref=chapter2 /> <!-- Missing quotes -->
        )";

        std::vector<std::string> expected = {"chapter1.xhtml"};
        REQUIRE(translator.extractIdrefs(spineContent) == expected);
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

TEST_CASE("parseManifestAndSpine") {
    TestableEpubTranslator translator;

    SECTION("Parses valid manifest and spine correctly") {
        std::vector<std::string> content = {
            "<package>", 
            "<manifest>", 
            "<item id=\"old\" href=\"old.xhtml\" />", 
            "</manifest>",
            "<spine>", 
            "<itemref idref=\"old\" />", 
            "</spine>",
            "</package>"
        };

        auto [manifest, spine] = translator.parseManifestAndSpine(content);

        REQUIRE(manifest.size() == 3); // <manifest>, item, </manifest>
        REQUIRE(spine.size() == 3);    // <spine>, itemref, </spine>
    }

    SECTION("Handles empty manifest and spine sections") {
        std::vector<std::string> content = {
            "<package>", 
            "<manifest></manifest>",
            "<spine></spine>", 
            "</package>"
        };

        auto [manifest, spine] = translator.parseManifestAndSpine(content);

        for (const auto& line : manifest) {
            std::cout << line << std::endl;
        }
        std::cout << "SPINE TEST" << std::endl;
        for (const auto& line : spine) {
            std::cout << line << std::endl;
        }

        REQUIRE(manifest.size() == 2); // <manifest>, </manifest>
        REQUIRE(spine.size() == 2);    // <spine>, </spine>
        // Print out the manifest and spine
        
    }

    SECTION("Returns empty manifest and spine when tags are missing") {
        std::vector<std::string> content = {
            "<package>", 
            "<metadata>Some metadata here</metadata>",
            "</package>"
        };

        auto [manifest, spine] = translator.parseManifestAndSpine(content);
        REQUIRE(manifest.empty());
        REQUIRE(spine.empty());
    }
}



TEST_CASE("updateManifest") {
    TestableEpubTranslator translator;

    SECTION("Adds new chapters correctly to manifest") {
        std::vector<std::string> manifest = { "<manifest>", "</manifest>" };
        std::vector<std::string> chapters = { "chapter1.xhtml", "chapter2.xhtml" };

        auto updatedManifest = translator.updateManifest(manifest, chapters);

        REQUIRE(updatedManifest.size() == 4); // <manifest>, 2 items, </manifest>
        REQUIRE(updatedManifest[1].find("chapter1.xhtml") != std::string::npos);
        REQUIRE(updatedManifest[2].find("chapter2.xhtml") != std::string::npos);
    }

    SECTION("Handles empty chapter list gracefully") {
        std::vector<std::string> manifest = { "<manifest>", "</manifest>" };
        std::vector<std::string> chapters = {};  // No chapters to add

        auto updatedManifest = translator.updateManifest(manifest, chapters);
        REQUIRE(updatedManifest.size() == 2); // No change
    }

    SECTION("Returns the original manifest if chapters are invalid (empty strings)") {
        std::vector<std::string> manifest = { "<manifest>", "</manifest>" };
        std::vector<std::string> chapters = { "", "" };  // Invalid chapter names

        auto updatedManifest = translator.updateManifest(manifest, chapters);
        REQUIRE(updatedManifest.size() == 4);  // Technically added empty items
        REQUIRE(updatedManifest[1].find("href=\"Text/\"") != std::string::npos);
    }

    SECTION("Does not crash on malformed manifest (missing closing tag)") {
        std::vector<std::string> manifest = { "<manifest>" };  // Missing </manifest>
        std::vector<std::string> chapters = { "chapter1.xhtml" };

        auto updatedManifest = translator.updateManifest(manifest, chapters);
        REQUIRE(updatedManifest.size() == 2); // Only <manifest> and added item
    }
}



TEST_CASE("updateSpine") {
    TestableEpubTranslator translator;

    SECTION("Adds new chapters correctly to spine") {
        std::vector<std::string> spine = { "<spine>", "</spine>" };
        std::vector<std::string> chapters = { "chapter1.xhtml", "chapter2.xhtml" };

        auto updatedSpine = translator.updateSpine(spine, chapters);

        REQUIRE(updatedSpine.size() == 4); // <spine>, 2 itemrefs, </spine>
        REQUIRE(updatedSpine[1].find("idref=\"chapter1\"") != std::string::npos);
        REQUIRE(updatedSpine[2].find("idref=\"chapter2\"") != std::string::npos);
    }

    SECTION("Handles empty chapter list gracefully") {
        std::vector<std::string> spine = { "<spine>", "</spine>" };
        std::vector<std::string> chapters = {};  // No chapters

        auto updatedSpine = translator.updateSpine(spine, chapters);
        REQUIRE(updatedSpine.size() == 2); // No change
    }

    SECTION("Handles invalid chapter names (empty strings)") {
        std::vector<std::string> spine = { "<spine>", "</spine>" };
        std::vector<std::string> chapters = { "", "" };  // Invalid names

        auto updatedSpine = translator.updateSpine(spine, chapters);
        REQUIRE(updatedSpine.size() == 4); // Added empty idrefs
        REQUIRE(updatedSpine[1].find("idref=\"chapter1\"") != std::string::npos);
    }

    SECTION("Handles malformed spine (missing closing tag)") {
        std::vector<std::string> spine = { "<spine>" };  // Missing </spine>
        std::vector<std::string> chapters = { "chapter1.xhtml" };

        auto updatedSpine = translator.updateSpine(spine, chapters);
        REQUIRE(updatedSpine.size() == 2); // <spine> and added itemref
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
