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
        std::cout << updatedManifest[0] << std::endl;
        std::cout << updatedManifest[1] << std::endl;
        std::cout << updatedManifest[2] << std::endl;


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

TEST_CASE("removeSection0001Tags removes specific tags from content.opf") {
    // Step 1: Create a temporary content.opf file
    std::filesystem::path tempFile = "temp_content.opf";
    std::ofstream tempOutput(tempFile);

    // Sample content with Section0001.xhtml tags
    std::string sampleContent = R"(
        <manifest>
            <item id="section1" href="Section0001.xhtml" media-type="application/xhtml+xml"/>
            <item id="section2" href="Section0002.xhtml" media-type="application/xhtml+xml"/>
        </manifest>
    )";

    tempOutput << sampleContent;
    tempOutput.close();

    // Step 2: Call the function to remove Section0001.xhtml tags
    TestableEpubTranslator translator;
    translator.removeSection0001Tags(tempFile);

    // Step 3: Read the modified file content
    std::ifstream tempInput(tempFile);
    std::stringstream buffer;
    buffer << tempInput.rdbuf();
    std::string modifiedContent = buffer.str();
    tempInput.close();

    // Step 4: Validate the output
    REQUIRE(modifiedContent.find("Section0001.xhtml") == std::string::npos);
    REQUIRE(modifiedContent.find("Section0002.xhtml") != std::string::npos);

    // Cleanup
    std::filesystem::remove(tempFile);
}

TEST_CASE("formatHTML handles various HTML inputs") {
    TestableEpubTranslator translator;

    SECTION("Formats simple valid HTML correctly") {
        std::string unformattedHTML = R"(<html><body><p>Hello, world!</p><div><span>Test</span></div></body></html>)";
        std::string formattedHTML = translator.formatHTML(unformattedHTML);

        REQUIRE(formattedHTML.find("<html>") != std::string::npos);
        REQUIRE(formattedHTML.find("<body>") != std::string::npos);
        REQUIRE(formattedHTML.find("<p>Hello, world!</p>") != std::string::npos);
        REQUIRE(formattedHTML.find("<div>") != std::string::npos);
        REQUIRE(formattedHTML.find("<span>Test</span>") != std::string::npos);
        REQUIRE(formattedHTML.find("</body>") != std::string::npos);
        REQUIRE(formattedHTML.find("</html>") != std::string::npos);
    }

    SECTION("Handles invalid HTML by auto-correcting structure") {
        std::string invalidHTML = R"(<html><body><p>Unclosed paragraph)";
        std::string formattedHTML = translator.formatHTML(invalidHTML);

        REQUIRE(!formattedHTML.empty());
        REQUIRE(formattedHTML.find("<p>Unclosed paragraph</p>") != std::string::npos); // Auto-closing expected
        REQUIRE(formattedHTML.find("</body>") != std::string::npos);
        REQUIRE(formattedHTML.find("</html>") != std::string::npos);
    }
    
    SECTION("Handles completely invalid input gracefully") {
        std::string invalidHTML = "%%% INVALID HTML $$$";
        std::string formattedHTML = translator.formatHTML(invalidHTML);
    
        std::cout << "FORMATTED HTML" << std::endl;
        std::cout << formattedHTML << std::endl;
    
        // Instead of expecting an empty string, verify the recovered structure
        REQUIRE(!formattedHTML.empty());
        REQUIRE(formattedHTML.find("<html>") != std::string::npos);
        REQUIRE(formattedHTML.find("<body>") != std::string::npos);
        REQUIRE(formattedHTML.find("<p>%%% INVALID HTML $$$</p>") != std::string::npos);
    }

}

TEST_CASE("updateNavXHTML correctly updates the TOC") {
    TestableEpubTranslator translator;

    // Create a temporary nav.xhtml file for testing
    std::filesystem::path tempNavPath = "temp_nav.xhtml";
    std::ofstream navFile(tempNavPath);
    navFile << R"(
        <nav epub:type="toc">
            <ol>
                <li><a href="existing.xhtml">Existing Chapter</a></li>
            </ol>
        </nav>
    )";
    navFile.close();

    std::vector<std::string> newChapters = {"chapter1.xhtml", "chapter2.xhtml"};

    translator.updateNavXHTML(tempNavPath, newChapters);

    // Read back the file to verify changes
    std::ifstream updatedNav(tempNavPath);
    std::string content((std::istreambuf_iterator<char>(updatedNav)),
                        std::istreambuf_iterator<char>());
    updatedNav.close();

    REQUIRE(content.find("chapter1.xhtml") != std::string::npos);
    REQUIRE(content.find("chapter2.xhtml") != std::string::npos);
    REQUIRE(content.find("Existing Chapter") != std::string::npos);

    // Cleanup
    std::filesystem::remove(tempNavPath);
}

TEST_CASE("copyImages correctly copies image files") {
    TestableEpubTranslator translator;

    // Setup: Create temp source and destination directories
    std::filesystem::path sourceDir = "temp_source";
    std::filesystem::path destDir = "temp_dest";
    std::filesystem::create_directory(sourceDir);
    std::filesystem::create_directory(destDir);

    // Create dummy image files
    std::ofstream(sourceDir / "image1.jpg").put('a');
    std::ofstream(sourceDir / "image2.png").put('b');
    std::ofstream(sourceDir / "text.txt").put('c');  // Should NOT be copied

    translator.copyImages(sourceDir, destDir);

    REQUIRE(std::filesystem::exists(destDir / "image1.jpg"));
    REQUIRE(std::filesystem::exists(destDir / "image2.png"));
    REQUIRE_FALSE(std::filesystem::exists(destDir / "text.txt"));  // Should not exist

    // Cleanup
    std::filesystem::remove_all(sourceDir);
    std::filesystem::remove_all(destDir);
}

TEST_CASE("replaceFullWidthSpaces correctly handles full-width spaces") {
    TestableEpubTranslator translator;

    SECTION("Replaces a single full-width space with a normal space") {
        xmlNodePtr parent = xmlNewNode(nullptr, BAD_CAST "test");
        xmlNodePtr node = xmlNewText(BAD_CAST "Hello　World");  // Full-width space (U+3000)
        xmlAddChild(parent, node);

        translator.replaceFullWidthSpaces(node);

        std::string result = reinterpret_cast<const char*>(node->content);
        REQUIRE(result == "Hello World");

        xmlFreeNode(parent);  // Cleanup
    }

    SECTION("Replaces multiple full-width spaces in the text") {
        xmlNodePtr parent = xmlNewNode(nullptr, BAD_CAST "test");
        xmlNodePtr node = xmlNewText(BAD_CAST "Hello　World　Test　Case");  // Multiple U+3000 spaces
        xmlAddChild(parent, node);

        translator.replaceFullWidthSpaces(node);

        std::string result = reinterpret_cast<const char*>(node->content);
        REQUIRE(result == "Hello World Test Case");

        xmlFreeNode(parent);  // Cleanup
    }

    SECTION("Handles text without any full-width spaces") {
        xmlNodePtr parent = xmlNewNode(nullptr, BAD_CAST "test");
        xmlNodePtr node = xmlNewText(BAD_CAST "Hello World");  // No full-width spaces
        xmlAddChild(parent, node);

        translator.replaceFullWidthSpaces(node);

        std::string result = reinterpret_cast<const char*>(node->content);
        REQUIRE(result == "Hello World");  // Should remain unchanged

        xmlFreeNode(parent);  // Cleanup
    }

    SECTION("Handles an empty text node gracefully") {
        xmlNodePtr parent = xmlNewNode(nullptr, BAD_CAST "test");
        xmlNodePtr node = xmlNewText(BAD_CAST "");  // Empty content
        xmlAddChild(parent, node);

        translator.replaceFullWidthSpaces(node);

        std::string result = reinterpret_cast<const char*>(node->content);
        REQUIRE(result == "");  // Should remain unchanged

        xmlFreeNode(parent);  // Cleanup
    }

    SECTION("Handles null node gracefully without crashing") {
        xmlNodePtr nullNode = nullptr;

        // Should not crash
        REQUIRE_NOTHROW(translator.replaceFullWidthSpaces(nullNode));
    }

    SECTION("Handles node with null content gracefully") {
        xmlNodePtr parent = xmlNewNode(nullptr, BAD_CAST "test");
        xmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "child");  // No content
        xmlAddChild(parent, node);

        // Should not crash
        REQUIRE_NOTHROW(translator.replaceFullWidthSpaces(node));

        xmlFreeNode(parent);  // Cleanup
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

TEST_CASE("stripHtmlTags correctly removes HTML tags from strings") {
    TestableEpubTranslator translator;

    SECTION("Removes simple HTML tags") {
        std::string input = "<p>Hello World</p>";
        std::string expected = "Hello World";

        REQUIRE(translator.stripHtmlTags(input) == expected);
    }

    SECTION("Handles nested HTML tags") {
        std::string input = "<div><strong>Bold Text</strong> with <em>italic</em> text.</div>";
        std::string expected = "Bold Text with italic text.";

        REQUIRE(translator.stripHtmlTags(input) == expected);
    }

    SECTION("Handles self-closing tags") {
        std::string input = "Line break here<br/>and continue.";
        std::string expected = "Line break hereand continue.";

        REQUIRE(translator.stripHtmlTags(input) == expected);
    }

    SECTION("Handles attributes inside tags") {
        std::string input = "<a href='https://example.com'>Click Here</a>";
        std::string expected = "Click Here";

        REQUIRE(translator.stripHtmlTags(input) == expected);
    }

    SECTION("Handles text without any HTML tags") {
        std::string input = "without tags.";
        std::string expected = "without tags.";

        REQUIRE(translator.stripHtmlTags(input) == expected);
    }

    SECTION("Handles empty input string") {
        std::string input = "";
        std::string expected = "";

        REQUIRE(translator.stripHtmlTags(input) == expected);
    }

    SECTION("Handles malformed HTML tags gracefully") {
        std::string input = "<div><b>Unclosed tags";
        std::string expected = "Unclosed tags";

        REQUIRE(translator.stripHtmlTags(input) == expected);
    }

    SECTION("Removes multiple consecutive tags") {
        std::string input = "<p><br/><br/>Multiple breaks</p>";
        std::string expected = "Multiple breaks";

        REQUIRE(translator.stripHtmlTags(input) == expected);
    }
}

TEST_CASE("readChapterFile reads file content correctly") {
    TestableEpubTranslator translator;

    SECTION("Reads content from a valid file") {
        std::filesystem::path tempFile = "test_chapter.html";
        std::ofstream file(tempFile);
        file << "<html><body><p>Hello, World!</p></body></html>";
        file.close();

        std::string content;
        REQUIRE_NOTHROW(content = translator.readChapterFile(tempFile));  // Correct usage
        REQUIRE(content == "<html><body><p>Hello, World!</p></body></html>");

        std::filesystem::remove(tempFile);  // Cleanup
    }

    SECTION("Throws exception when file does not exist") {
        std::filesystem::path nonExistentFile = "non_existent.html";
        REQUIRE_THROWS_AS(translator.readChapterFile(nonExistentFile), std::runtime_error);
    }
}

TEST_CASE("parseHtmlDocument parses valid HTML content") {
    TestableEpubTranslator translator;

    SECTION("Parses valid HTML content successfully") {
        std::string htmlContent = "<html><body><p>Test</p></body></html>";

        htmlDocPtr doc = nullptr;
        REQUIRE_NOTHROW(doc = translator.parseHtmlDocument(htmlContent));  // Ensure no exception is thrown
        REQUIRE(doc != nullptr);  // Validate that the document was parsed successfully

        xmlFreeDoc(doc);  // Cleanup to avoid memory leaks
    }

    SECTION("Throws exception on invalid HTML content") {
        std::string invalidContent = "";  // Empty content
        REQUIRE_THROWS_AS(translator.parseHtmlDocument(invalidContent), std::runtime_error);
    }
}

TEST_CASE("cleanNodes removes unwanted tags") {
    TestableEpubTranslator translator;

    std::string htmlContent = "<html><body><p>Keep this</p><span>Remove this</span></body></html>";
    htmlDocPtr doc = htmlReadMemory(htmlContent.c_str(), htmlContent.size(), NULL, "UTF-8", 0);
    REQUIRE(doc != nullptr);

    xmlNodeSetPtr nodes = translator.extractNodesFromDoc(doc);
    REQUIRE(nodes != nullptr);

    translator.cleanNodes(nodes);

    std::string cleanedContent = translator.serializeDocument(doc);
    REQUIRE(cleanedContent.find("<span>") == std::string::npos);  // <span> should be removed
    REQUIRE(cleanedContent.find("<p>Keep this</p>") != std::string::npos);

    xmlFreeDoc(doc);
}

TEST_CASE("serializeDocument correctly serializes HTML content") {
    TestableEpubTranslator translator;

    std::string htmlContent = "<html><body><p>Sample Text</p></body></html>";
    htmlDocPtr doc = htmlReadMemory(htmlContent.c_str(), htmlContent.size(), NULL, "UTF-8", 0);
    REQUIRE(doc != nullptr);

    std::string serializedContent;

    REQUIRE_NOTHROW(serializedContent = translator.serializeDocument(doc));

    REQUIRE(serializedContent.find("Sample Text") != std::string::npos);

    xmlFreeDoc(doc);
}

TEST_CASE("writeChapterFile writes content to a file correctly") {
    TestableEpubTranslator translator;
    std::filesystem::path tempFile = "output_chapter.html";

    std::string content = "<html><body><p>New Content</p></body></html>";
    REQUIRE_NOTHROW(translator.writeChapterFile(tempFile, content));

    std::ifstream file(tempFile);
    std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    REQUIRE(fileContent == content);

    std::filesystem::remove(tempFile); // Cleanup
}

TEST_CASE("extractNodesFromDoc extracts <p> and <img> tags correctly") {
    TestableEpubTranslator translator;

    std::string htmlContent = R"(<html><body><p>Paragraph 1</p><img src="image.png"/><p>Paragraph 2</p></body></html>)";
    htmlDocPtr doc = htmlReadMemory(htmlContent.c_str(), htmlContent.size(), NULL, "UTF-8", 0);
    REQUIRE(doc != nullptr);

    xmlNodeSetPtr nodes = translator.extractNodesFromDoc(doc);
    REQUIRE(nodes != nullptr);
    REQUIRE(nodes->nodeNr == 3);  // Two <p> and one <img>

    xmlFreeDoc(doc);
}

TEST_CASE("cleanChapter works correctly") {
    TestableEpubTranslator translator;

    SECTION("Cleans unwanted tags and replaces full-width spaces") {
        std::filesystem::path testFile = "test.xhtml";
        std::ofstream file(testFile);
        file << R"(
            <html>
                <body>
                    <p>Hello　world!</p>   <!-- Full-width space between 'Hello' and 'world!' -->
                    <br />
                    <ruby>Kanji<rt>Reading</rt></ruby>
                </body>
            </html>
        )";
        file.close();

        // Perform the cleaning operation
        translator.cleanChapter(testFile);

        // Read the cleaned file content
        std::ifstream cleanedFile(testFile);
        std::string content((std::istreambuf_iterator<char>(cleanedFile)), std::istreambuf_iterator<char>());
        cleanedFile.close();

        // Get rid of whitespace to prevent assertion failures due to formatting
        content.erase(std::remove_if(content.begin(), content.end(), ::isspace), content.end());

        // Assertions
        REQUIRE(content.find("<br>") == std::string::npos);
        REQUIRE(content.find("<ruby>") == std::string::npos);
        REQUIRE(content.find("\xE3\x80\x80") == std::string::npos);  // Full-width space
        REQUIRE(content.find("Helloworld!") != std::string::npos);  // Space normalized

        // std::filesystem::remove(testFile); // Cleanup
    }

    SECTION("Handles files with no unwanted tags gracefully") {
        std::filesystem::path testFile = "clean.xhtml";
        std::ofstream file(testFile);
        file << R"(
            <html>
                <body>
                    <p>Clean content without tags.</p>
                </body>
            </html>
        )";
        file.close();

        translator.cleanChapter(testFile);

        std::ifstream cleanedFile(testFile);
        std::string content((std::istreambuf_iterator<char>(cleanedFile)), std::istreambuf_iterator<char>());
        cleanedFile.close();

        REQUIRE(content.find("Clean content without tags.") != std::string::npos);
        REQUIRE(content.find("<ruby>") == std::string::npos);  // Should still be absent
        REQUIRE(content.find("<br>") == std::string::npos);    // No accidental modifications

        std::filesystem::remove(testFile); // Cleanup
    }

    SECTION("Handles malformed HTML gracefully") {
        std::filesystem::path testFile = "malformed.xhtml";
        std::ofstream file(testFile);
        file << R"(
            <html>
                <body>
                    <p>Unclosed tag
                    <br>
                </body>
            </html>
        )";
        file.close();

        REQUIRE_NOTHROW(translator.cleanChapter(testFile));  // Should handle without crashing

        std::ifstream cleanedFile(testFile);
        std::string content((std::istreambuf_iterator<char>(cleanedFile)), std::istreambuf_iterator<char>());
        cleanedFile.close();

        REQUIRE(content.find("<br>") == std::string::npos);  // Ensure <br> is removed
        REQUIRE(content.find("Unclosed tag") != std::string::npos);

        std::filesystem::remove(testFile); // Cleanup
    }
}

TEST_CASE("processImgTag extracts image filename correctly") {
    TestableEpubTranslator translator;

    SECTION("Extracts filename from img tag with src attribute") {
        // Create a mock <img> node
        xmlNodePtr imgNode = xmlNewNode(nullptr, BAD_CAST "img");
        xmlNewProp(imgNode, BAD_CAST "src", BAD_CAST "images/photo.png");

        tagData tag = translator.processImgTag(imgNode, 1, 0);

        REQUIRE(tag.tagId == IMG_TAG);
        REQUIRE(tag.position == 1);
        REQUIRE(tag.chapterNum == 0);
        REQUIRE(tag.text == "photo.png"); // Expecting only the filename

        xmlFreeNode(imgNode); // Cleanup
    }

    SECTION("Handles img tag without src attribute gracefully") {
        xmlNodePtr imgNode = xmlNewNode(nullptr, BAD_CAST "img"); // No src attribute

        tagData tag = translator.processImgTag(imgNode, 2, 1);

        REQUIRE(tag.tagId == IMG_TAG);
        REQUIRE(tag.position == 2);
        REQUIRE(tag.chapterNum == 1);
        REQUIRE(tag.text.empty()); // Should be empty since there's no src

        xmlFreeNode(imgNode); // Cleanup
    }
}

TEST_CASE("processPTag extracts and cleans text content from p tags") {
    TestableEpubTranslator translator;

    SECTION("Extracts plain text from p tag with nested HTML") {
        xmlNodePtr pNode = xmlNewNode(nullptr, BAD_CAST "p");
        xmlNodeSetContent(pNode, BAD_CAST "Hello <b>World</b>!");

        tagData tag = translator.processPTag(pNode, 0, 0);

        REQUIRE(tag.tagId == P_TAG);
        REQUIRE(tag.position == 0);
        REQUIRE(tag.chapterNum == 0);
        REQUIRE(tag.text == "Hello World!"); // HTML tags should be stripped

        xmlFreeNode(pNode); // Cleanup
    }

    SECTION("Handles empty p tag gracefully") {
        xmlNodePtr pNode = xmlNewNode(nullptr, BAD_CAST "p"); // Empty <p> tag

        tagData tag = translator.processPTag(pNode, 3, 2);

        REQUIRE(tag.tagId == P_TAG);
        REQUIRE(tag.position == 3);
        REQUIRE(tag.chapterNum == 2);
        REQUIRE(tag.text.empty()); // No content should result in an empty string

        xmlFreeNode(pNode); // Cleanup
    }
}

TEST_CASE("readFileUtf8 reads UTF-8 encoded file content correctly") {
    TestableEpubTranslator translator;

    SECTION("Reads content from a valid UTF-8 file") {
        std::filesystem::path tempFile = "test_utf8.txt";
        std::ofstream file(tempFile, std::ios::binary);
        file << "Hello, 世界!"; // Including UTF-8 characters
        file.close();

        // Step 1: Ensure no exception is thrown using try-catch
        std::string content;
        try {
            content = translator.readFileUtf8(tempFile);
        } catch (const std::exception& e) {
            FAIL("Exception thrown: " << e.what());
        }

        // Step 2: Validate the content
        REQUIRE(content == "Hello, 世界!");

        std::filesystem::remove(tempFile);
    }

    SECTION("Throws exception when file does not exist") {
        std::filesystem::path nonExistentFile = "non_existent_file.txt";
        
        // Check that reading a non-existent file throws an exception
        REQUIRE_THROWS_AS(translator.readFileUtf8(nonExistentFile), std::runtime_error);
    }

    SECTION("Reads empty file without issues") {
        std::filesystem::path emptyFile = "empty.txt";
        std::ofstream file(emptyFile, std::ios::binary); // Create an empty file
        file.close();

        std::string content;
        try {
            content = translator.readFileUtf8(emptyFile);
        } catch (const std::exception& e) {
            FAIL("Exception thrown: " << e.what());
        }

        REQUIRE(content.empty());

        std::filesystem::remove(emptyFile);
    }

    SECTION("Handles file with large UTF-8 content") {
        std::filesystem::path largeFile = "large_utf8.txt";
        std::ofstream file(largeFile, std::ios::binary);
        std::string largeContent(10000, 'A'); // 10,000 'A' characters
        file << largeContent;
        file.close();

        std::string content;
        try {
            content = translator.readFileUtf8(largeFile);
        } catch (const std::exception& e) {
            FAIL("Exception thrown: " << e.what());
        }

        REQUIRE(content == largeContent); // Ensure large content is read correctly

        std::filesystem::remove(largeFile);
    }
}

TEST_CASE("extractTags extracts <p> and <img> tags correctly from chapters") {
    TestableEpubTranslator translator;

    SECTION("Extracts both <p> and <img> tags from a valid chapter file") {
        std::filesystem::path chapterFile = "chapter1.xhtml";
        std::ofstream file(chapterFile);
        file << R"(
            <html>
                <body>
                    <p>Hello, World!</p>
                    <img src="images/photo.png" />
                    <p>Another paragraph.</p>
                </body>
            </html>
        )";
        file.close();

        std::vector<std::filesystem::path> chapters = { chapterFile };
        std::vector<tagData> tags = translator.extractTags(chapters);

        REQUIRE(tags.size() == 3); // 2 <p> tags + 1 <img> tag

        // Check for <p> tag 1
        REQUIRE(tags[0].tagId == P_TAG);
        REQUIRE(tags[0].text == "Hello, World!");
        REQUIRE(tags[0].position == 0);
        REQUIRE(tags[0].chapterNum == 0);

        // Check for <img> tag
        REQUIRE(tags[1].tagId == IMG_TAG);
        REQUIRE(tags[1].text == "photo.png"); // Only filename should be extracted
        REQUIRE(tags[1].position == 1);
        REQUIRE(tags[1].chapterNum == 0);

        // Check for <p> tag 2
        REQUIRE(tags[2].tagId == P_TAG);
        REQUIRE(tags[2].text == "Another paragraph.");
        REQUIRE(tags[2].position == 2);
        REQUIRE(tags[2].chapterNum == 0);

        std::filesystem::remove(chapterFile); // Cleanup
    }

    SECTION("Handles empty chapter gracefully") {
        std::filesystem::path emptyChapter = "empty.xhtml";
        std::ofstream file(emptyChapter);
        file << R"(<html><body></body></html>)"; // No <p> or <img> tags
        file.close();

        std::vector<std::filesystem::path> chapters = { emptyChapter };
        std::vector<tagData> tags = translator.extractTags(chapters);

        REQUIRE(tags.empty()); // Should return an empty vector

        std::filesystem::remove(emptyChapter); // Cleanup
    }

    SECTION("Handles malformed HTML without crashing") {
        std::filesystem::path malformedChapter = "malformed.xhtml";
        std::ofstream file(malformedChapter);
        file << R"(<html><body><p>Unclosed paragraph)"; // Missing closing </p> and </body>
        file.close();
    
        std::vector<std::filesystem::path> chapters = { malformedChapter };
    
        std::vector<tagData> tags;
        try {
            tags = translator.extractTags(chapters);
        } catch (const std::exception& e) {
            FAIL("Exception thrown: " << e.what());
        }
    
        REQUIRE(tags.size() == 1); // Should still extract the unclosed <p> tag
        REQUIRE(tags[0].tagId == P_TAG);
        REQUIRE(tags[0].text == "Unclosed paragraph");
    
        std::filesystem::remove(malformedChapter); // Cleanup
    }

    SECTION("Extracts tags from multiple chapters") {
        std::filesystem::path chapter1 = "chapter1.xhtml";
        std::ofstream file1(chapter1);
        file1 << R"(<html><body><p>Chapter 1 Content</p></body></html>)";
        file1.close();

        std::filesystem::path chapter2 = "chapter2.xhtml";
        std::ofstream file2(chapter2);
        file2 << R"(<html><body><img src="images/cover.jpg"/></body></html>)";
        file2.close();

        std::vector<std::filesystem::path> chapters = { chapter1, chapter2 };
        std::vector<tagData> tags = translator.extractTags(chapters);

        REQUIRE(tags.size() == 2);

        // Chapter 1
        REQUIRE(tags[0].tagId == P_TAG);
        REQUIRE(tags[0].text == "Chapter 1 Content");
        REQUIRE(tags[0].chapterNum == 0);

        // Chapter 2
        REQUIRE(tags[1].tagId == IMG_TAG);
        REQUIRE(tags[1].text == "cover.jpg");
        REQUIRE(tags[1].chapterNum == 1);

        std::filesystem::remove(chapter1);
        std::filesystem::remove(chapter2); // Cleanup
    }
}

TEST_CASE("exportEpub creates a valid EPUB file", "[exportEpub]") {
    std::string tempExportPath = "test_export";
    std::string tempOutputDir = "test_output";
    std::string epubPath = tempOutputDir + "/output.epub";

    // Create test directories
    std::filesystem::create_directories(tempExportPath);
    std::filesystem::create_directories(tempOutputDir);

    // Create sample files
    std::string file1 = tempExportPath + "/test1.txt";
    std::string file2 = tempExportPath + "/test2.txt";

    std::ofstream(file1) << "Sample content 1";
    std::ofstream(file2) << "Sample content 2";

    // Call the function to generate the EPUB
    TestableEpubTranslator translator;
    translator.exportEpub(tempExportPath, tempOutputDir);

    // Check if the EPUB file is created
    REQUIRE(std::filesystem::exists(epubPath));

    // Open the EPUB file as a ZIP archive
    int err = 0;
    zip_t* archive = zip_open(epubPath.c_str(), ZIP_RDONLY, &err);
    REQUIRE(archive != nullptr);

    // Check if the expected files exist inside the archive
    REQUIRE(zip_name_locate(archive, "test1.txt", ZIP_FL_ENC_UTF_8) >= 0);
    REQUIRE(zip_name_locate(archive, "test2.txt", ZIP_FL_ENC_UTF_8) >= 0);

    // Close the ZIP archive
    zip_close(archive);

    // Cleanup test directories and files
    std::filesystem::remove_all(tempExportPath);
    std::filesystem::remove_all(tempOutputDir);
}

TEST_CASE("TestableEpubTranslator::removeUnwantedTags removes specific tags while preserving content", "[removeUnwantedTags]") {
    TestableEpubTranslator translator;

    SECTION("Removes <br>, <i>, <span>, <ruby>, and <rt> tags correctly") {
        std::string inputHTML = R"(
            <html>
                <body>
                    <p>This is<br/> a <i>test</i> with <span>unwanted</span> <ruby>tags<rt>note</rt></ruby>.</p>
                </body>
            </html>
        )";

        // Parse the input HTML
        htmlDocPtr doc = htmlReadMemory(inputHTML.c_str(), inputHTML.size(), NULL, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
        REQUIRE(doc != nullptr);

        // Get the root node and apply the function
        xmlNodePtr root = xmlDocGetRootElement(doc);
        translator.removeUnwantedTags(root);

        // Convert the modified document back to a string
        xmlChar* output = nullptr;
        int size = 0;
        xmlDocDumpMemory(doc, &output, &size);
        std::string result;
        if (output) {
            result.assign(reinterpret_cast<const char*>(output), size);
            xmlFree(output);
        }

        // Clean up
        xmlFreeDoc(doc);

        // Debug output for verification
        std::cout << "Modified HTML:\n" << result << std::endl;

        // Check that unwanted tags are removed
        REQUIRE(result.find("<br") == std::string::npos);
        REQUIRE(result.find("<i>") == std::string::npos);
        REQUIRE(result.find("<span>") == std::string::npos);
        REQUIRE(result.find("<ruby>") == std::string::npos);
        REQUIRE(result.find("<rt>") == std::string::npos);

        // Ensure content remains intact
        REQUIRE(result.find("This is a test with unwanted tags.") != std::string::npos);
    }

    SECTION("Handles nested unwanted tags") {
        std::string nestedHTML = R"(
            <div>
                <span><i>Nested</i> content</span> with <ruby>multiple<rt>tags</rt></ruby>
            </div>
        )";

        htmlDocPtr doc = htmlReadMemory(nestedHTML.c_str(), nestedHTML.size(), NULL, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
        REQUIRE(doc != nullptr);

        xmlNodePtr root = xmlDocGetRootElement(doc);
        translator.removeUnwantedTags(root);

        xmlChar* output = nullptr;
        int size = 0;
        xmlDocDumpMemory(doc, &output, &size);
        std::string result;
        if (output) {
            result.assign(reinterpret_cast<const char*>(output), size);
            xmlFree(output);
        }

        xmlFreeDoc(doc);

        std::cout << "Modified Nested HTML:\n" << result << std::endl;

        REQUIRE(result.find("<i>") == std::string::npos);
        REQUIRE(result.find("<span>") == std::string::npos);
        REQUIRE(result.find("<ruby>") == std::string::npos);
        REQUIRE(result.find("<rt>") == std::string::npos);

        REQUIRE(result.find("Nested content with multiple") != std::string::npos);
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
