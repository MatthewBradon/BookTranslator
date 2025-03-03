#include "EpubTranslator.h"




std::filesystem::path EpubTranslator::searchForOPFFiles(const std::filesystem::path& directory) {
    try {
        // Iterate through the directory and its subdirectories
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".opf") {
                return entry.path();
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << "\n";
    }

    return std::filesystem::path();
}


bool EpubTranslator::containsJapanese(const std::string& text) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(text.c_str());
    size_t length = text.size();

    // UTF-8 encoding rules:
    // 1-byte ASCII characters: 0xxxxxxx
    // 2-byte characters: 110xxxxx 10xxxxxx
    // 3-byte characters: 1110xxxx 10xxxxxx 10xxxxxx
    // 4-byte characters: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

    for (size_t i = 0; i < length; ) {
        uint32_t codepoint = 0; // codepoint is a unique number assigned to each character
        size_t numBytes = 0; // Number of bytes in the UTF-8 character

        // Determine number of bytes in this UTF-8 character
        if (bytes[i] < 0x80) { // 1-byte ASCII
            codepoint = bytes[i];

            numBytes = 1;

        }  else if ((bytes[i] & 0xE0) == 0xC0) { // 2-byte sequence
            if (i + 1 >= length) return false; // Invalid UTF-8 because of missing bytes

            codepoint = ((bytes[i] & 0x1F) << 6) | // Take the last 5 bits of the first byte
                        (bytes[i + 1] & 0x3F); // Take the last 6 bits of the second byte
            
            numBytes = 2;

        } else if ((bytes[i] & 0xF0) == 0xE0) { // 3-byte sequence (Most Japanese )
            
            if (i + 2 >= length) return false; // Invalid UTF-8 because of missing bytes

            codepoint = ((bytes[i] & 0x0F) << 12) | // Take the last 4 bits of the first byte
                        ((bytes[i + 1] & 0x3F) << 6) |  // Take the last 6 bits of the second byte
                        (bytes[i + 2] & 0x3F); // Take the last 6 bits of the third byte
            
            
            numBytes = 3;

        } else if ((bytes[i] & 0xF8) == 0xF0) { // 4-byte sequence (Rare for Japanese)
            
            if (i + 3 >= length) return false; // Invalid UTF-8 because of missing bytes

            codepoint = ((bytes[i] & 0x07) << 18) | // Take the last 3 bits of the first byte
                        ((bytes[i + 1] & 0x3F) << 12) | // Take the last 6 bits of the second byte
                        ((bytes[i + 2] & 0x3F) << 6) | // Take the last 6 bits of the third byte
                        (bytes[i + 3] & 0x3F); // Take the last 6 bits of the fourth byte
            
                        
            numBytes = 4;

        } else {
            return false; // Invalid UTF-8
        }

        // Check if the codepoint is in Japanese ranges
        if ((codepoint >= 0x3040 && codepoint <= 0x309F) ||  // Hiragana
            (codepoint >= 0x30A0 && codepoint <= 0x30FF) ||  // Katakana
            (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||  // CJK Unified (Kanji)
            (codepoint >= 0xFF66 && codepoint <= 0xFF9F)) {  // Half-width Katakana
            return true;
        }

        i += numBytes; // Move to next UTF-8 character
    }

    return false;
}

std::string EpubTranslator::extractSpineContent(const std::string& content) {
    std::regex spinePattern(R"(<spine\b[^>]*>([\s\S]*?)<\/spine>)");
    std::smatch match;
    if (std::regex_search(content, match, spinePattern)) {
        return match[1].str();
    }
    throw std::runtime_error("No <spine> tag found in the OPF file.");
}

std::vector<std::string> EpubTranslator::extractIdrefs(const std::string& spineContent) {
    std::vector<std::string> idrefs;
    std::regex idrefPattern(R"(idref\s*=\s*"([^\"]*)\")");
    auto begin = std::sregex_iterator(spineContent.begin(), spineContent.end(), idrefPattern);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        std::string idref = (*it)[1].str();
        // if (idref.find(".xhtml") == std::string::npos) {
        //     idref += ".xhtml";
        // }
        idrefs.push_back(idref);
    }
    return idrefs;
}

std::vector<std::string> EpubTranslator::getSpineOrder(const std::filesystem::path& directory) {
    std::vector<std::string> spineOrder;

    try {
        std::ifstream opfFile(directory);

        if (!opfFile.is_open()) {
            std::cerr << "Failed to open OPF file: " << directory << "\n";
            return spineOrder;  // Return the empty vector
        }

        std::string content((std::istreambuf_iterator<char>(opfFile)), std::istreambuf_iterator<char>());
        opfFile.close();

        std::string spineContent = extractSpineContent(content);
        spineOrder = extractIdrefs(spineContent);


    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << "\n";
    }

    return spineOrder;  // Ensure we always return a vector
}

std::vector<std::filesystem::path> EpubTranslator::getAllXHTMLFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> xhtmlFiles;

    try {
        // Iterate through the directory and its subdirectories
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && (entry.path().extension() == ".xhtml")  ) {
                xhtmlFiles.push_back(entry.path());
            }

            // Check for html files
            if (entry.is_regular_file() && (entry.path().extension() == ".html")  ) {
                xhtmlFiles.push_back(entry.path());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << "\n";
    }

    return xhtmlFiles;
}

std::vector<std::filesystem::path> EpubTranslator::sortXHTMLFilesBySpineOrder(const std::vector<std::filesystem::path>& xhtmlFiles, const std::vector<std::string>& spineOrder, std::vector<std::pair<std::string, std::string>> manifestMappingIds)  {
    std::vector<std::filesystem::path> sortedXHTMLFiles;

    for (const auto& idref : spineOrder) {
        // Find the corresponding href from the manifestMappingIds
        auto it = std::find_if(manifestMappingIds.begin(), manifestMappingIds.end(), 
            [&](const std::pair<std::string, std::string>& pair) {
                return pair.first == idref;
            });

        if (it != manifestMappingIds.end()) {
            std::filesystem::path manifestFilename = std::filesystem::path(it->second).filename();

            for (const auto& xhtmlFile : xhtmlFiles) {
                if (xhtmlFile.filename() == manifestFilename) {
                    sortedXHTMLFiles.push_back(xhtmlFile);
                    break; // Stop after finding the first match
                }
            }
        }
    }

    return sortedXHTMLFiles;
}

std::vector<std::pair<std::string, std::string>> EpubTranslator::extractManifestIds(const std::vector<std::string>& manifestItems) {
    std::vector<std::pair<std::string, std::string>> idToFileMapping;

    // Separate regex patterns for extracting id and href
    std::regex idPattern(R"(\bid\s*=\s*\"([^\"]+)\")");
    std::regex hrefPattern(R"(\bhref\s*=\s*\"([^\"]+)\")");

    for (const std::string& item : manifestItems) {
        std::smatch idMatch, hrefMatch;
        std::string id, href;

        // Extract id attribute
        if (std::regex_search(item, idMatch, idPattern)) {
            id = idMatch[1].str();
        }

        // Extract href attribute
        if (std::regex_search(item, hrefMatch, hrefPattern)) {
            href = hrefMatch[1].str();
            // // Find the last occurence of . get the substring for the file extention check if its html change it to xhtml
            // std::string hrefTemp = hrefMatch[1].str();

            // if(hrefTemp.substr(hrefTemp.find_last_of(".") + 1) == "html") {
            //     href = hrefTemp.substr(0, hrefTemp.find_last_of(".")) + ".xhtml";
            // } else {
            //     href = hrefTemp;
            // }

            
        }

        // Ensure both id and href are found before adding to the mapping
        if (!id.empty() && !href.empty()) {
            idToFileMapping.emplace_back(id, href);
        }
    }

    // Print the extracted id-to-file mapping
    for (const auto& pair : idToFileMapping) {
        std::cout << "ID: " << pair.first << ", File: " << pair.second << std::endl;
    }

    return idToFileMapping;
}

std::pair<std::vector<std::string>, std::vector<std::string>> EpubTranslator::parseManifestAndSpine(const std::vector<std::string>& content) 
{
    std::vector<std::string> manifest, spine;

    // Step 1: Concatenate the content into a single string.
    // Note: If you actually want newlines preserved, use "combinedContent += line + '\n';" instead.
    std::string combinedContent;
    for (const auto& line : content) {
        combinedContent += line;  // Removes newlines
    }

    // Step 2: Regex patterns to extract the entire <manifest>...</manifest> and <spine>...</spine> blocks.
    std::regex manifestBlockPattern(R"(<manifest\b[^>]*>([\s\S]*?)</manifest>)");
    std::regex spineBlockPattern(R"(<spine\b[^>]*>([\s\S]*?)</spine>)");
    std::regex tagPattern(R"(<[^>]+>)");  // Matches any XML/HTML tag

    std::smatch match;

    // Step 3: Extract and parse the <manifest> block.
    if (std::regex_search(combinedContent, match, manifestBlockPattern)) {
        manifest.push_back("\n<manifest>\n");  // Add opening tag

        std::string manifestContent = match[1].str();  // Content between <manifest> and </manifest>
        auto tagBegin = std::sregex_iterator(manifestContent.begin(), manifestContent.end(), tagPattern);
        auto tagEnd = std::sregex_iterator();

        for (auto it = tagBegin; it != tagEnd; ++it) {
            manifest.push_back(it->str() + "\n");  // Add each tag within <manifest>
        }

        manifest.push_back("\n</manifest>\n");  // Add closing tag
    }

    // Step 4: Extract and parse the <spine> block.
    if (std::regex_search(combinedContent, match, spineBlockPattern)) {
        spine.push_back("\n<spine>\n");  // Add opening tag

        std::string spineContent = match[1].str();  // Content between <spine> and </spine>
        auto tagBegin = std::sregex_iterator(spineContent.begin(), spineContent.end(), tagPattern);
        auto tagEnd = std::sregex_iterator();

        for (auto it = tagBegin; it != tagEnd; ++it) {
            spine.push_back(it->str() + "\n");  // Add each tag within <spine>
        }

        spine.push_back("\n</spine>\n");  // Add closing tag
    }

    return {manifest, spine};
}

// Update manifest section with new chapters
std::vector<std::string> EpubTranslator::updateManifest(const std::vector<std::pair<std::string, std::string>>& manifestMappingIds) {
    std::vector<std::string> updatedManifest;
    
    // Start the manifest block
    updatedManifest.push_back("<manifest>\n");

    // Append new manifest items
    for (const auto& [id, file] : manifestMappingIds) {
        std::string filename = std::filesystem::path(file).filename().string();
        std::string updatedPath;

        // Skip css files
        if (filename.size() > 4 && filename.compare(filename.size() - 4, 4, ".css") == 0) {
            continue;
        }
        // Skip js files
        if (filename.size() > 3 && filename.compare(filename.size() - 3, 3, ".js") == 0) {
            continue;
        }

        if (filename.size() > 6 && filename.compare(filename.size() - 6, 6, ".xhtml") == 0) {
            updatedPath = "Text/" + filename;
        } else if (filename.size() > 5 && filename.compare(filename.size() - 5, 5, ".html") == 0) {
            updatedPath = "Text/" + filename;
        } else if (filename.size() > 4 && 
                   (filename.compare(filename.size() - 4, 4, ".jpg") == 0 ||
                    filename.compare(filename.size() - 5, 5, ".jpeg") == 0 ||
                    filename.compare(filename.size() - 4, 4, ".png") == 0)) {
            updatedPath = "Images/" + filename;
        } else {
            updatedPath = filename; // Keep original path for other file types
        }

        // Create manifest entry
        std::string manifestEntry = "   <item id=\"" + id + "\" href=\"" + updatedPath + "\" />\n";
        updatedManifest.push_back(manifestEntry);
    }

    // Close the manifest block
    updatedManifest.push_back("</manifest>\n");

    return updatedManifest;
}


// Update spine section with new chapters
std::vector<std::string> EpubTranslator::updateSpine(const std::vector<std::string>& chapters, const std::vector<std::pair<std::string, std::string>>& manifestMappingIds) {
    std::vector<std::string> updatedSpine;
    
    // Start the spine block
    updatedSpine.push_back("<spine>\n");

    // Iterate over manifestMappingIds and include only XHTML file IDs
    for (const auto& [id, file] : manifestMappingIds) {
        // Check if the file is an XHTML file
        if (file.size() > 6 && file.compare(file.size() - 6, 6, ".xhtml") == 0) {
            std::string spineEntry = "  <itemref idref=\"" + id + "\" />\n";
            updatedSpine.push_back(spineEntry);
        }
        // Check if the file is an html file
        if (file.size() > 5 && file.compare(file.size() - 5, 5, ".html") == 0) {
            std::string spineEntry = "  <itemref idref=\"" + id + "\" />\n";
            updatedSpine.push_back(spineEntry);
        }
    }

    // Close the spine block
    updatedSpine.push_back("</spine>\n");

    return updatedSpine;
}


void EpubTranslator::removeSection0001Tags(const std::filesystem::path& contentOpfPath) {
    // Step 1: Read the entire file into a single string
    std::ifstream inputFile(contentOpfPath);
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open content.opf file!" << "\n";
        return;
    }

    std::stringstream buffer;
    buffer << inputFile.rdbuf();  // Read the entire file content
    std::string content = buffer.str();
    inputFile.close();

    // Step 2: Regex to match tags containing "Section0001.xhtml"
    std::regex sectionRegex(R"(<[^>]*Section0001\.xhtml[^>]*>)");

    // Step 3: Remove all matching tags
    content = std::regex_replace(content, sectionRegex, "");

    // Step 4: Write the modified content back to the file
    std::ofstream outputFile(contentOpfPath);
    if (!outputFile.is_open()) {
        std::cerr << "Failed to open content.opf file for writing!" << "\n";
        return;
    }

    outputFile << content;
    outputFile.close();
}

void EpubTranslator::updateContentOpf(const std::vector<std::string>& epubChapterList, const std::filesystem::path& contentOpfPath, const std::vector<std::pair<std::string, std::string>>& manifestMappingIds) {
    std::ifstream inputFile(contentOpfPath);
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open content.opf file!" << "\n";
        return;
    }

    std::vector<std::string> fileContent;
    std::string line;

    while (std::getline(inputFile, line)) {
        fileContent.push_back(line);
    }
    inputFile.close();

    try {

        std::vector<std::string> updatedManifest = updateManifest(manifestMappingIds);

        for (const auto& line : updatedManifest) {
            std::cout << line << std::endl;
        }

        std::vector<std::string> updatedSpine = updateSpine(epubChapterList, manifestMappingIds);

        std::ostringstream updatedContentStream;
        bool insideManifest = false, insideSpine = false;

        for (const auto& fileLine : fileContent) {
            // Skip original manifest and spine sections
            if (fileLine.find("<manifest") != std::string::npos) {
                insideManifest = true;                
                continue;
            }
            if (fileLine.find("</manifest>") != std::string::npos) {
                insideManifest = false;
                continue;
            }
            if (fileLine.find("<spine") != std::string::npos) {
                insideSpine = true;
                continue;
            }
            if (fileLine.find("</spine>") != std::string::npos) {
                insideSpine = false;
                continue;
            }
            if (insideManifest || insideSpine) {
                continue;  // Skip lines inside the manifest and spine blocks
            }

            // Insert updated manifest and spine after </metadata>
            if (fileLine.find("</metadata>") != std::string::npos) {
                updatedContentStream << fileLine << "\n";
                for (const auto& manifestLine : updatedManifest) {
                    updatedContentStream << manifestLine;
                }
                for (const auto& spineLine : updatedSpine) {
                    updatedContentStream << spineLine;
                }
            } else {
                updatedContentStream << fileLine << "\n";
            }
        }

        std::ofstream outputFile(contentOpfPath);
        if (!outputFile.is_open()) {
            std::cerr << "Failed to open content.opf file for writing!" << "\n";
            return;
        }

        outputFile << updatedContentStream.str();
        outputFile.close();

        removeSection0001Tags(contentOpfPath);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

bool EpubTranslator::make_directory(const std::filesystem::path& path) {
    try {

        // Check if the directory already exists
        if (std::filesystem::exists(path)) {
            // std::cerr << "Directory already exists: " << path << "\n";
            return true;
        }
        // Use create_directories to create the directory and any necessary parent directories
        return std::filesystem::create_directories(path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating directory: " << e.what() << "\n";
        return false;
    }
}

bool EpubTranslator::unzip_file(const std::string& zipPath, const std::string& outputDir) {
    int err = 0;
    zip* archive = zip_open(zipPath.c_str(), ZIP_RDONLY, &err);
    if (archive == nullptr) {
        std::cerr << "Error opening ZIP archive: " << zipPath << "\n";
        return false;
    }

    zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    for (zip_uint64_t i = 0; i < numEntries; ++i) {
        const char* name = zip_get_name(archive, i, 0);
        if (name == nullptr) {
            std::cerr << "Error getting file name in ZIP archive." << "\n";
            zip_close(archive);
            return false;
        }

        std::filesystem::path outputPath = std::filesystem::path(outputDir) / name;

        // If the entry is a directory, create it and continue
        if (name[strlen(name) - 1] == '/') {
            std::filesystem::create_directories(outputPath);
            continue;  // Skip file extraction
        }

        // Ensure the parent directory exists
        std::filesystem::create_directories(outputPath.parent_path());

        zip_file* file = zip_fopen_index(archive, i, 0);
        if (file == nullptr) {
            std::cerr << "Error opening file in ZIP archive: " << name << "\n";
            zip_close(archive);
            return false;
        }

        std::ofstream outputFile(outputPath, std::ios::binary);
        if (!outputFile.is_open()) {
            std::cerr << "Error creating output file: " << outputPath << "\n";
            zip_fclose(file);
            zip_close(archive);
            return false;
        }

        char buffer[4096];
        zip_int64_t bytesRead;
        while ((bytesRead = zip_fread(file, buffer, sizeof(buffer))) > 0) {
            outputFile.write(buffer, bytesRead);
        }

        outputFile.close();
        zip_fclose(file);
    }

    zip_close(archive);
    return true;
}

void EpubTranslator::exportEpub(const std::string& exportPath, const std::string& outputDir) {
    // Check if the exportPath directory exists
    if (!std::filesystem::exists(exportPath)) {
        std::cerr << "Export directory does not exist: " << exportPath << "\n";
        return;
    }

    if (!std::filesystem::exists(outputDir)) {
        std::cerr << "Ouput path does not exist: " << outputDir << "\n";
        return;
    }

    // Create the output Epub file path
    std::string epubPath = outputDir + "/output.epub";

    // Create a ZIP archive
    zip_t* archive = zip_open(epubPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, nullptr);
    if (!archive) {
        std::cerr << "Error creating ZIP archive: " << epubPath << "\n";
        return;
    }

    // Add all files in the exportPath directory to the ZIP archive
    for (const auto& entry : std::filesystem::recursive_directory_iterator(exportPath)) {
        if (entry.is_regular_file()) {
            std::filesystem::path filePath = entry.path();
            std::filesystem::path relativePath = filePath.lexically_relative(exportPath);

            // Convert file paths to UTF-8 encoded strings
            std::string filePathUtf8 = filePath.u8string();
            std::string relativePathUtf8 = relativePath.u8string();

            // Create a zip_source_t from the file
            zip_source_t* source = zip_source_file(archive, filePathUtf8.c_str(), 0, 0);
            if (source == nullptr) {
                std::cerr << "Error creating zip_source_t for file: " << filePath << "\n";
                zip_discard(archive);
                return;
            }

            // Add the file to the ZIP archive
            zip_int64_t index = zip_file_add(archive, relativePathUtf8.c_str(), source, ZIP_FL_ENC_UTF_8);
            if (index < 0) {
                std::cerr << "Error adding file to ZIP archive: " << filePath << "\n";
                zip_source_free(source);
                zip_discard(archive);
                return;
            }
        }
    }

    // Close the ZIP archive
    if (zip_close(archive) < 0) {
        std::cerr << "Error closing ZIP archive: " << epubPath << "\n";
        return;
    }

    std::cout << "Epub file created: " << epubPath << "\n";
}

void EpubTranslator::updateNavXHTML(std::filesystem::path navXHTMLPath, const std::vector<std::string>& epubChapterList) {
    std::ifstream navXHTMLFile(navXHTMLPath);
    if (!navXHTMLFile.is_open()) {
        std::cerr << "Failed to open nav.xhtml file!" << "\n";
        return;
    }

    std::vector<std::string> fileContent;  // Store the entire file content
    std::string line;
    bool insideTocNav = false;
    bool insideOlTag = false;

    while (std::getline(navXHTMLFile, line)) {
        fileContent.push_back(line);

        if (line.find("<nav epub:type=\"toc\"") != std::string::npos) {
            insideTocNav = true;
        }

        if (line.find("<ol>") != std::string::npos && insideTocNav) {
            insideOlTag = true;
            // Append new chapter entries
            for (size_t i = 0; i < epubChapterList.size(); ++i) {
                std::string chapterFilename = std::filesystem::path(epubChapterList[i]).filename().string();
                fileContent.push_back("    <li><a href=\"" + chapterFilename + "\">Chapter " + std::to_string(i + 1) + "</a></li>");
            }
        }

        if (line.find("</ol>") != std::string::npos && insideTocNav) {
            insideOlTag = false;
        }

        if (line.find("</nav>") != std::string::npos && insideTocNav) {
            insideTocNav = false;
        }
    }

    navXHTMLFile.close();

    // Write the updated content back to the file
    std::ofstream outputFile(navXHTMLPath);
    if (!outputFile.is_open()) {
        std::cerr << "Failed to open nav.xhtml file for writing!" << "\n";
        return;
    }

    for (const auto& fileLine : fileContent) {
        outputFile << fileLine << "\n";
    }

    outputFile.close();
}

void EpubTranslator::copyImages(const std::filesystem::path& sourceDir, const std::filesystem::path& destinationDir) {
    try {
        // Create the destination directory if it doesn't exist
        if (!std::filesystem::exists(destinationDir)) {
            std::filesystem::create_directories(destinationDir);
        }

        // Traverse the source directory
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (extension == ".jpg" || extension == ".jpeg" || extension == ".png") {
                    std::filesystem::path destinationPath = destinationDir / entry.path().filename();
                    std::filesystem::copy(entry.path(), destinationPath, std::filesystem::copy_options::overwrite_existing);
                }
            }
        }
        std::cout << "Image files copied successfully." << "\n";
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

void EpubTranslator::replaceFullWidthSpaces(xmlNodePtr node) {
    if (node == nullptr || node->content == nullptr || node->type != XML_TEXT_NODE) {
        return;
    }

    std::string textContent = reinterpret_cast<const char*>(node->content);
    std::string fullWidthSpace = "\xE3\x80\x80"; // UTF-8 encoding of \u3000
    std::string normalSpace = " ";

    size_t pos = 0;
    while ((pos = textContent.find(fullWidthSpace, pos)) != std::string::npos) {
        textContent.replace(pos, fullWidthSpace.length(), normalSpace);
        pos += normalSpace.length();
    }
    // Update the node's content with the modified text
    xmlNodeSetContent(node, reinterpret_cast<const xmlChar*>(textContent.c_str()));
}

void EpubTranslator::removeUnwantedTags(xmlNodePtr node) {
    if (!node) return;

    xmlNodePtr current = node;
    while (current != nullptr) {
        xmlNodePtr nextNode = current->next;  // Save next node before potential modification

        // Recursively process all child nodes first
        if (current->children) {
            removeUnwantedTags(current->children);
        }

        // Replace full-width spaces in text nodes
        if (current->type == XML_TEXT_NODE) {
            replaceFullWidthSpaces(current);
        }

        if (current->type == XML_ELEMENT_NODE) {
            if (xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("br")) == 0 ||
                xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("i")) == 0 || 
                xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("span")) == 0 || 
                xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("ruby")) == 0 ||
                xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("rt")) == 0) {

                if (xmlStrcmp(current->name, reinterpret_cast<const xmlChar*>("rt")) == 0) {
                    // For <rt> tags, delete both the tag and its content
                    xmlUnlinkNode(current);
                    xmlFreeNode(current);
                } else {
                    // For other unwanted tags, move their children before removal
                    xmlNodePtr child = current->children;
                    while (child) {
                        xmlNodePtr nextChild = child->next;
                        xmlUnlinkNode(child);

                        // Move child directly without inserting extra spaces
                        xmlAddPrevSibling(current, child);
                        child = nextChild;
                    }

                    // Remove the unwanted tag itself
                    xmlUnlinkNode(current);
                    xmlFreeNode(current);
                }
            }
        }

        current = nextNode;  // Move to the next sibling
    }
}

std::string EpubTranslator::readChapterFile(const std::filesystem::path& chapterPath) {
    std::ifstream file(chapterPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + chapterPath.string());
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

htmlDocPtr EpubTranslator::parseHtmlDocument(const std::string& content) {
    htmlDocPtr doc = htmlReadMemory(content.c_str(), content.size(), NULL, "UTF-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) {
        throw std::runtime_error("Failed to parse HTML content.");
    }
    return doc;
}

void EpubTranslator::cleanNodes(xmlNodeSetPtr nodes) {
    if (nodes) {
        for (int i = 0; i < nodes->nodeNr; ++i) {
            xmlNodePtr node = nodes->nodeTab[i];
            removeUnwantedTags(node);
        }
    }
}

std::string EpubTranslator::serializeDocument(htmlDocPtr doc) {
    xmlBufferPtr buffer = xmlBufferCreate();
    if (!buffer) {
        throw std::runtime_error("Failed to create XML buffer.");
    }

    int result = xmlNodeDump(buffer, doc, xmlDocGetRootElement(doc), 0, 1);
    if (result == -1) {
        xmlBufferFree(buffer);
        throw std::runtime_error("Failed to serialize XML document.");
    }

    std::string output(reinterpret_cast<const char*>(xmlBufferContent(buffer)), xmlBufferLength(buffer));
    xmlBufferFree(buffer);
    return output;
}

void EpubTranslator::writeChapterFile(const std::filesystem::path& chapterPath, const std::string& content) {
    std::ofstream outFile(chapterPath);
    if (!outFile.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + chapterPath.string());
    }
    outFile << content;
}

xmlNodeSetPtr EpubTranslator::extractNodesFromDoc(htmlDocPtr doc) {
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (!xpathCtx) return nullptr;

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//p | //img"), xpathCtx);
    xmlXPathFreeContext(xpathCtx);

    return (xpathObj) ? xpathObj->nodesetval : nullptr;
}

void EpubTranslator::cleanChapter(const std::filesystem::path& chapterPath) {
    try {
        std::string content = readChapterFile(chapterPath);

        htmlDocPtr doc = parseHtmlDocument(content);

        xmlNodeSetPtr nodes = extractNodesFromDoc(doc);

        cleanNodes(nodes);

        std::string cleanedContent = serializeDocument(doc);

        writeChapterFile(chapterPath, cleanedContent);

        // Cleanup
        xmlFreeDoc(doc);

    } catch (const std::exception& e) {
        std::cerr << "Error cleaning chapter: " << e.what() << "\n";
    }
}

std::string EpubTranslator::stripHtmlTags(const std::string& input) {
    // Regular expression to match HTML tags
    std::regex tagRegex("<[^>]*>");
    // Replace all occurrences of HTML tags with an empty string
    return std::regex_replace(input, tagRegex, "");
}

tagData EpubTranslator::processImgTag(xmlNodePtr node, int position, int chapterNum) {
    tagData tag;
    tag.tagId = IMG_TAG;
    tag.position = position;
    tag.chapterNum = chapterNum;

    // Set of attributes that could contain image references
    std::unordered_set<std::string> imageAttributes = {"src", "xlink:href", "href", "data", "srcset", "poster"};

    xmlAttr* attr = node->properties;
    while (attr) {
        if (attr->children && attr->children->content) {
            std::string attrName = reinterpret_cast<const char*>(attr->name);
            std::string attrValue = reinterpret_cast<char*>(attr->children->content);

            std::cout << "Attribute name: " << attrName << " | Value: " << attrValue << std::endl;

            // Check if the attribute is in the imageAttributes set
            if (imageAttributes.find(attrName) != imageAttributes.end()) {
                if (attrValue.find(".jpg") != std::string::npos || 
                    attrValue.find(".jpeg") != std::string::npos || 
                    attrValue.find(".png") != std::string::npos || 
                    attrValue.find(".gif") != std::string::npos || 
                    attrValue.find(".bmp") != std::string::npos || 
                    attrValue.find(".svg") != std::string::npos) {
                    
                    // Extract only the filename
                    tag.text = std::filesystem::path(attrValue).filename().string();
                    std::cout << "Extracted filename: " << tag.text << std::endl;
                    break; // Stop after finding the first valid image reference
                }
            }
        }
        attr = attr->next;
    }

    return tag;
}

tagData EpubTranslator::processPTag(xmlNodePtr node, int position, int chapterNum) {
    tagData tag;
    tag.tagId = P_TAG;
    tag.position = position;
    tag.chapterNum = chapterNum;

    xmlChar* textContent = xmlNodeGetContent(node);
    if (textContent) {
        tag.text = stripHtmlTags(reinterpret_cast<char*>(textContent));
        xmlFree(textContent);
    }
    return tag;
}

std::string EpubTranslator::readFileUtf8(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filePath.string());
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<tagData> EpubTranslator::extractTags(const std::vector<std::filesystem::path>& chapterPaths) {
    std::vector<tagData> bookTags;
    int chapterNum = 0;

    for (const auto& chapterPath : chapterPaths) {
        try {
            std::string data = readFileUtf8(chapterPath);
            htmlDocPtr doc = parseHtmlDocument(data);
            if (!doc) continue;


            xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
            if (!xpathCtx) {
                xmlFreeDoc(doc);
                continue;
            }
        
            xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//*"), xpathCtx);
            xmlXPathFreeContext(xpathCtx);
        
            xmlNodeSetPtr nodes = (xpathObj) ? xpathObj->nodesetval : nullptr;
            if (!nodes) {
                xmlFreeDoc(doc);
                continue;
            }
            int position = 0;
            for (int i = 0; i < nodes->nodeNr; ++i) {
                xmlNodePtr node = nodes->nodeTab[i];
                if (node->type == XML_ELEMENT_NODE) {
                    if (xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("p")) == 0) {
                        tagData tag = processPTag(node, position, chapterNum);
                        if (!tag.text.empty()) {
                            bookTags.push_back(tag);
                            position++;
                        }
                    } else {
                        tagData tag = processImgTag(node, position, chapterNum);
                        if (!tag.text.empty()) {
                            bookTags.push_back(tag);
                            position++;
                        }
                    }
                }
            }

            xmlFreeDoc(doc);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
        chapterNum++;
    }
    return bookTags;
}

size_t EpubTranslator::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

std::string EpubTranslator::uploadDocumentToDeepL(const std::string& filePath, const std::string& deepLKey) {
    CURL* curl;
    CURLcode res;
    std::string response_string;
    std::string api_url = "https://api-free.deepl.com/v2/document";
    std::string auth_key = "DeepL-Auth-Key " + deepLKey;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: " + auth_key).c_str());

        // Prepare the multipart form data
        curl_mime* form = curl_mime_init(curl);
        curl_mimepart* field = curl_mime_addpart(form);
        curl_mime_name(field, "target_lang");
        curl_mime_data(field, "EN", CURL_ZERO_TERMINATED); // Change to desired target language
        field = curl_mime_addpart(form);
        curl_mime_name(field, "file");
        curl_mime_filedata(field, filePath.c_str());  // Path to the file you want to upload

        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        // Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "Curl upload request failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_mime_free(form);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();

    // Assuming the response contains document_id and document_key, extract them
    try {
        std::cout << "Document upload response: " << response_string << std::endl;
        nlohmann::json jsonResponse = nlohmann::json::parse(response_string);
        std::string document_id = jsonResponse["document_id"];
        std::string document_key = jsonResponse["document_key"];
        return document_id + "|" + document_key;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Error parsing document upload response: " << e.what() << std::endl;
        return "";
    }
}

std::string EpubTranslator::checkDocumentStatus(const std::string& document_id, const std::string& document_key, const std::string& deepLKey) {
    CURL* curl;
    CURLcode res;
    std::string response_string;
    std::string api_url = "https://api-free.deepl.com/v2/document/" + document_id;
    std::string auth_key = "DeepL-Auth-Key " + deepLKey;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: " + auth_key).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // Create JSON payload
        nlohmann::json jsonPayload;
        jsonPayload["document_key"] = document_key;
        std::string json_data = jsonPayload.dump();

        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        // Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "Curl status check request failed: " << curl_easy_strerror(res) << std::endl;
        }

        std::cout << "Document status response: " << response_string << std::endl;

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();

    return response_string;  // You can parse this response to check if status == "done"
}

std::string EpubTranslator::downloadTranslatedDocument(const std::string& document_id, const std::string& document_key, const std::string& deepLKey) {
    CURL* curl;
    CURLcode res;
    std::string response_string;
    std::string api_url = "https://api-free.deepl.com/v2/document/" + document_id + "/result";
    std::string auth_key = "DeepL-Auth-Key " + deepLKey;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: " + auth_key).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // Create JSON payload
        nlohmann::json jsonPayload;
        jsonPayload["document_key"] = document_key;
        std::string json_data = jsonPayload.dump();

        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        // Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "Curl download request failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();

    return response_string;
}

int EpubTranslator::handleDeepLRequest(const std::vector<tagData>& bookTags, const std::vector<std::filesystem::path>& spineOrderXHTMLFiles, std::string deepLKey) {
    
    std::vector<std::string> htmlStringVector;

    std::vector<std::vector<tagData>> chapterTags;
    // Divide bookTags into chapters chapterNum is the chapter number
    for (auto& tag : bookTags) {
        if (tag.chapterNum >= chapterTags.size()) {
            chapterTags.resize(tag.chapterNum + 1);
        }
        chapterTags[tag.chapterNum].push_back(tag);
    }

    // Build the position map for each chapter and position
    std::unordered_map<int, std::unordered_map<int, tagData*>> positionMap;

    for (size_t chapterNum = 0; chapterNum < chapterTags.size(); ++chapterNum) {
        for (auto& tag : chapterTags[chapterNum]) {
            positionMap[chapterNum][tag.position] = &tag;
        }
    }

    // Write out to the template EPUB
    std::string htmlHeader = R"(
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
    <title>)";

    std::string htmlFooter = R"(</body>
</html>)";

    for (size_t i = 0; i < spineOrderXHTMLFiles.size(); ++i) {
        std::string htmlString;
        htmlString += htmlHeader + spineOrderXHTMLFiles[i].filename().string() + "</title>\n</head>\n<body>\n";
        std::cout << "Chapter: " << i << "\n";
        // Write content-specific parts
        for (const auto& tag : chapterTags[i]) {
            if (tag.tagId == P_TAG) {
                htmlString += "\t<p>" + tag.text + "</p>\n";
            } else if (tag.tagId == IMG_TAG) {
                htmlString += "\t<img src=\"../Images/" + tag.text + "\" alt=\"\"/>\n";
            }
        }

        // Append footer
        htmlString += htmlFooter;
        htmlStringVector.push_back(htmlString);
    }

    std::cout << "HTML strings generated successfully." << "\n";

    if (!make_directory("testHTML")) {
        std::cerr << "Failed to create testHTML directory." << "\n";
        return 1;
    }

    for (size_t i = 0; i < htmlStringVector.size(); ++i) {
        
        std::ofstream outFile("testHTML/" + std::to_string(i) + ".html");
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << i << ".html" << "\n";
            return 1;
        }
        outFile << htmlStringVector[i];
        outFile.close();
    }

    std::vector<bool> htmlContainsPTagsVector(htmlStringVector.size(), false);

    for (size_t i = 0; i < htmlStringVector.size(); ++i) {
        if (htmlStringVector[i].find("<p>") != std::string::npos) {
            htmlContainsPTagsVector[i] = true;
            std::cout << "Chapter " << i << " contains <p> tags." << "\n";
        }
    }

    for (size_t i = 0; i < htmlStringVector.size(); ++i) {
        std::cout << "Inside for loop" << "\n";
        if (!htmlContainsPTagsVector[i]) {
            continue;
        }

        std::string chapterPath = "testHTML/" + std::to_string(i) + ".html";

        // return 0;

        std::string uploadResult = uploadDocumentToDeepL(chapterPath, deepLKey);

        if (uploadResult.empty()) {
            std::cerr << "Failed to upload document to DeepL." << "\n";
            return 1;
        }

        std::string document_id = uploadResult.substr(0, uploadResult.find('|'));
        std::string document_key = uploadResult.substr(uploadResult.find('|') + 1);

        std::cout << "Uploaded document. Document ID: " << document_id << ", Document Key: " << document_key << "\n";

        std::string status;
        bool isTranslationComplete = false;

        while (!isTranslationComplete) {
            status = checkDocumentStatus(document_id, document_key, deepLKey);
            // Parse the JSON response to check if status == "done"
            nlohmann::json jsonResponse = nlohmann::json::parse(status);
            if (jsonResponse["status"] == "done") {
                isTranslationComplete = true;
            } else {
                std::cout << "Translation in progress..." << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(5));  // Wait for 5 seconds before checking again
            }
        }

        std::string responseHTMLString = downloadTranslatedDocument(document_id, document_key, deepLKey);

        std::cout << responseHTMLString << "\n";

        htmlStringVector[i] = responseHTMLString;
        // Limit the number of translations for testing because of DeepL API limits
        // if (i == 9 ) {
        //     break;
        // }
    }

    // Create testHTML directory if it doesn't exist
    if (!make_directory("translatedHTML")) {
        std::cerr << "Failed to create testHTML directory." << "\n";
        return 1;
    }

    // Write the updated content to the XHTML files in directory testHTML
    for (size_t i = 0; i < htmlStringVector.size(); ++i) {
        
        std::ofstream outFile("translatedHTML/" + std::to_string(i) + ".xhtml");
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << i << ".xhtml" << "\n";
            return 1;
        }
        outFile << htmlStringVector[i];
        outFile.close();
    }

    // Go through all the translate XHTML and detect if there is any japanese text
    std::cout << "Detecting Japanese text in translated XHTML files..." << "\n";

    std::vector<std::filesystem::path> translatedXHTMLFiles;

    for (size_t i = 0; i < spineOrderXHTMLFiles.size(); ++i) {
        std::filesystem::path translatedFilePath = std::filesystem::path("translatedHTML/" + std::to_string(i) + ".xhtml");
        if (!std::filesystem::exists(translatedFilePath)) {
            std::cerr << "Translated file not found: " << translatedFilePath << "\n";
            return 1;
        }
        translatedXHTMLFiles.push_back(translatedFilePath);
    }

    std::vector<tagData> translatedTags = extractTags(translatedXHTMLFiles);

    std::vector<tagData> notTranslatedTags;

    for (const auto& tag : translatedTags) {
        if (tag.tagId == IMG_TAG) continue;

        if (containsJapanese(tag.text)) {
            notTranslatedTags.push_back(tag);
        }
    }

    if (notTranslatedTags.empty()) {
        std::cout << "No Japanese text detected in translated XHTML files." << "\n";
        return 0;
    } else {
        std::cout << "Japanese text detected in translated XHTML files:" << "\n";
        for (const auto& tag : notTranslatedTags) {
            std::cout << "Chapter: " << tag.chapterNum << " | Position: " << tag.position << " | Text: " << tag.text << "\n";
        }
    }

    std::cout << "Running local model translation for Japanese text" << "\n";

    std::string rawTagsPathString = "rawTags.txt";
    std::string translatedTagsPathString = "translatedTags.txt";

    // Write out a file of the raw tags
    std::filesystem::path rawTagsPath = rawTagsPathString;
    std::ofstream rawTagsFile(rawTagsPath);
    if (!rawTagsFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << rawTagsPath << "\n";
        return 1;
    }

    std::cout << "Writing raw tags to file: " << rawTagsPath << "\n";
    for (const auto& tag : notTranslatedTags) {
        auto chapterIt = positionMap.find(tag.chapterNum);
        if (chapterIt != positionMap.end()) {  // Check if chapter exists
            auto positionIt = chapterIt->second.find(tag.position);
            if (positionIt != chapterIt->second.end() && positionIt->second != nullptr) {  // Check if position exists
                std::string originalText = positionIt->second->text;
                rawTagsFile << tag.tagId << "," << tag.chapterNum << "," << tag.position << "," << originalText << "\n";
            } else {
                std::cerr << "Warning: Missing position in positionMap for Chapter: " 
                          << tag.chapterNum << ", Position: " << tag.position << "\n";
            }
        } else {
            std::cerr << "Warning: Missing chapter in positionMap for Chapter: " << tag.chapterNum << "\n";
        }
    }
    
    std::cout << "Raw tags written to file: " << rawTagsPath << "\n";

    rawTagsFile.close();




    //Start the multiprocessing translaton
    std::filesystem::path translationExe;
    std::string chapterNumberMode = "0";
    #if defined(__APPLE__)
        translationExe = "translation";

    #elif defined(_WIN32)
        translationExe = "translation.exe";
    #else
        std::cerr << "Unsupported platform!" << std::endl;
        return 1; // Or some other error handling
    #endif

    if (!std::filesystem::exists(translationExe)) {
        std::cerr << "Executable not found: " << translationExe << std::endl;
        return 1;
    }

    std::string translationExePath = translationExe.string();
    

    std::cout << "Before call to translation.py" << '\n';

    boost::process::ipstream pipe_stdout, pipe_stderr;

    try {
        boost::process::child c(
            translationExePath,
            rawTagsPathString,
            chapterNumberMode,
            boost::process::std_out > pipe_stdout, 
            boost::process::std_err > pipe_stderr
        );

        // Threads to handle asynchronous reading
        std::thread stdout_thread([&pipe_stdout]() {
            std::string line;
            while (std::getline(pipe_stdout, line)) {
                std::cout << "Python stdout: " << line << "\n";
            }
        });

        std::thread stderr_thread([&pipe_stderr]() {
            std::string line;
            while (std::getline(pipe_stderr, line)) {
                std::cerr << "Python stderr: " << line << "\n";
            }
        });

        c.wait(); // Wait for process completion

        stdout_thread.join();
        stderr_thread.join();

        if (c.exit_code() == 0) {
            std::cout << "Translation Python script executed successfully." << "\n";
        } else {
            std::cerr << "Translation Python script exited with code: " << c.exit_code() << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << "\n";
    }

    std::cout << "After call to translation.py" << '\n';

    std::vector<decodedData> decodedDataVector;
    std::ifstream file(translatedTagsPathString);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << translatedTagsPathString << "\n";
        return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream lineStream(line);
        std::string chapterNumStr, positionStr, text;

        // Extract chapterNum (first value)
        if (!std::getline(lineStream, chapterNumStr, ',')) continue;

        // Extract position (second value)
        if (!std::getline(lineStream, positionStr, ',')) continue;

        // The rest of the line is text
        std::getline(lineStream, text);

        // Convert chapterNum and position to integers
        try {
            decodedData data;
            data.chapterNum = std::stoi(chapterNumStr);
            data.position = std::stoi(positionStr);
            data.output = text;
            // Store the extracted data
            decodedDataVector.push_back(data);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << " - " << e.what() << "\n";
        }
    }
    file.close();

    // Create translatedPositionsMap out of translatedTags
    std::vector<std::vector<tagData>> translatedChapterTags;
    std::unordered_map<int, std::unordered_map<int, tagData*>> translatedPositionMap;



    for (auto& tag : translatedTags) {
        if (tag.chapterNum >= translatedChapterTags.size()) {
            translatedChapterTags.resize(tag.chapterNum + 1);
        }
        translatedChapterTags[tag.chapterNum].push_back(tag);
    }

    for (size_t chapterNum = 0; chapterNum < translatedChapterTags.size(); ++chapterNum) {
        for (auto& tag : translatedChapterTags[chapterNum]) {
            translatedPositionMap[chapterNum][tag.position] = &tag;
        }
    }

    for (const auto& data : decodedDataVector) {
        auto chapterIt = translatedPositionMap.find(data.chapterNum);
        if (chapterIt != translatedPositionMap.end()) {
            auto positionIt = chapterIt->second.find(data.position);
            if (positionIt != chapterIt->second.end() && positionIt->second != nullptr) {
                positionIt->second->text = data.output;
            }
        }
    }

    // Looop through translatedChapterTags and check if there is any untranslated text
    for (size_t i = 0; i < translatedChapterTags.size(); ++i) {
        for (const auto& tag : translatedChapterTags[i]) {
            if (tag.tagId == P_TAG && containsJapanese(tag.text)) {
                std::cerr << "Untranslated Japanese text detected in Chapter: " << i << " | Position: " << tag.position << " | Text: " << tag.text << "\n";
            }
        }
    }

    std::cout << "Writing translated XHTML files..." << "\n";

    // Write out to the template EPUB
    for (size_t i = 0; i < spineOrderXHTMLFiles.size(); ++i) {
        std::filesystem::path outputPath = "export/OEBPS/Text/" + spineOrderXHTMLFiles[i].filename().string();
        std::ofstream outFile(outputPath);
        std::cout << "Writing to: " << outputPath << "\n";
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << outputPath << "\n";
            return 1;
        }

        // Write pre-built header
        outFile << htmlHeader << spineOrderXHTMLFiles[i].filename().string() << "</title>\n</head>\n<body>\n";

        if (i > translatedChapterTags.size()) {
            outFile << htmlFooter;
            outFile.close();
            continue;
        }

        // Write content-specific parts
        for (const auto& tag : translatedChapterTags[i]) {
            if (tag.tagId == P_TAG) {
                outFile << "<p>" << tag.text << "</p>\n";
            } else if (tag.tagId == IMG_TAG) {
                outFile << "<img src=\"../Images/" << tag.text << "\" alt=\"\"/>\n";
            }
        }

        // Write pre-built footer
        outFile << htmlFooter;
        outFile.close();
    }

    return 0; 
}

void EpubTranslator::addTitleAndAuthor(const char* filename, const std::string& title, const std::string& author) {
    xmlDocPtr doc = xmlReadFile(filename, NULL, 0);
    if (!doc) {
        std::cerr << "Failed to parse " << filename << std::endl;
        return;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        std::cerr << "Empty document!" << std::endl;
        xmlFreeDoc(doc);
        return;
    }

    // Find the <metadata> node
    xmlNodePtr metadataNode = nullptr;
    for (xmlNodePtr node = root->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, BAD_CAST "metadata") == 0) {
            metadataNode = node;
            break;
        }
    }

    if (!metadataNode) {
        std::cerr << "No <metadata> found in XML!" << std::endl;
        xmlFreeDoc(doc);
        return;
    }

    // Ensure the Dublin Core namespace is defined
    xmlNsPtr dcNamespace = xmlSearchNsByHref(doc, metadataNode, BAD_CAST "http://purl.org/dc/elements/1.1/");
    if (!dcNamespace) {
        dcNamespace = xmlNewNs(metadataNode, BAD_CAST "http://purl.org/dc/elements/1.1/", BAD_CAST "dc");
    }

    // Overwrite <dc:title>
    xmlNodePtr titleNode = nullptr;
    for (xmlNodePtr node = metadataNode->children; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, BAD_CAST "title") == 0 &&
            node->ns && xmlStrcmp(node->ns->href, BAD_CAST "http://purl.org/dc/elements/1.1/") == 0) {
            titleNode = node;
            break;
        }
    }

    if (titleNode) {
        xmlNodeSetContent(titleNode, BAD_CAST title.c_str());  // Overwrite existing title
    } else {
        xmlNewChild(metadataNode, dcNamespace, BAD_CAST "title", BAD_CAST title.c_str());
    }

    // Remove existing <dc:creator> nodes
    xmlNodePtr cur = metadataNode->children;
    while (cur) {
        xmlNodePtr next = cur->next;
        if (cur->type == XML_ELEMENT_NODE && xmlStrcmp(cur->name, BAD_CAST "creator") == 0 &&
            cur->ns && xmlStrcmp(cur->ns->href, BAD_CAST "http://purl.org/dc/elements/1.1/") == 0) {
            xmlUnlinkNode(cur);
            xmlFreeNode(cur);
        }
        cur = next;
    }

    // Add new <dc:creator>
    xmlNewChild(metadataNode, dcNamespace, BAD_CAST "creator", BAD_CAST author.c_str());

    // Save the updated XML
    xmlSaveFormatFileEnc(filename, doc, "UTF-8", 1);
    xmlFreeDoc(doc);
    xmlCleanupParser();
}

int EpubTranslator::run(const std::string& epubToConvert, const std::string& outputEpubPath, int localModel, const std::string& deepLKey, std::string langcode) {
    std::cout << "langcode: " << langcode << "\n";
    std::cout << "localModel: " << localModel << "\n";
    std::filesystem::path currentDirPath = std::filesystem::current_path();

    std::cout << "Running the EPUB conversion process..." << "\n";
    std::cout << "epubToConvert: " << epubToConvert << "\n";
    std::cout << "outputEpubPath: " << outputEpubPath << "\n";

    std::string unzippedPath = "unzipped";
    std::string templatePath = "export";
    std::string templateEpub = "rawEpub/template.epub";

    // Check if the unzipped directory already exists
    if (std::filesystem::exists(unzippedPath)) {
        std::cout << "Unzipped directory already exists. Deleting it..." << "\n";
        std::filesystem::remove_all(unzippedPath);
    }

    // Check if the export directory already exists
    if (std::filesystem::exists(templatePath)) {
        std::cout << "Export directory already exists. Deleting it..." << "\n";
        std::filesystem::remove_all(templatePath);
    }


    // Start the timer
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "START" << "\n";

    // Create the output directory if it doesn't exist
    if (!make_directory(unzippedPath)) {
        std::cerr << "Failed to create output directory: " << unzippedPath << "\n";
        return 1;
    }

    // Unzip the EPUB file
    if (!unzip_file(epubToConvert, unzippedPath)) {
        std::cerr << "Failed to unzip EPUB file: " << epubToConvert << "\n";
        return 1;
    }

    std::cout << "EPUB file unzipped successfully to: " << unzippedPath << "\n";

    // Create the template directory if it doesn't exist
    if (!make_directory(templatePath)) {
        std::cerr << "Failed to create template directory: " << templatePath << "\n";
        return 1;
    }

    // Unzip the template EPUB file
    if (!unzip_file(templateEpub, templatePath)) {
        std::cerr << "Failed to unzip EPUB file: " << templateEpub << "\n";
        return 1;
    }

    std::cout << "EPUB file unzipped successfully to: " << templatePath << "\n";

    
    std::filesystem::path contentOpfPath = searchForOPFFiles(std::filesystem::path(unzippedPath));

    if (contentOpfPath.empty()) {
        std::cerr << "No OPF file found in the unzipped EPUB directory." << "\n";
        return 1;
    }

    std::cout << "Found OPF file: " << contentOpfPath << "\n";

    std::vector<std::string> spineOrder = getSpineOrder(contentOpfPath);

    // Print the spine order
    std::cout << "Spine Order:" << "\n";
    for (const auto& item : spineOrder) {
        std::cout << item << "\n";
    }

    std::cout << "After getSpineOrder" << "\n";

    if (spineOrder.empty()) {
        std::cerr << "No spine order found in the OPF file." << "\n";
        return 1;
    }

    std::ifstream inputFile(contentOpfPath);
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open content.opf file!" << "\n";
        return 1;
    }

    std::vector<std::string> fileContent;
    std::string fileline;

    while (std::getline(inputFile, fileline)) {
        fileContent.push_back(fileline);
    }
    inputFile.close();


    // Extract spine and manifest from the OPF file
    std::pair<std::vector<std::string>, std::vector<std::string>> spineAndManifest = parseManifestAndSpine(fileContent);

    std::vector<std::string> manifest = spineAndManifest.first;
    std::vector<std::string> spine = spineAndManifest.second;

    // Print the spine and manifest
    std::cout << "Manifest:" << "\n";
    for (const auto& item : spineAndManifest.first) {
        std::cout << item << "\n";
    }

    std::cout << "Spine:" << "\n";
    for (const auto& item : spineAndManifest.second) {
        std::cout << item << "\n";
    }

    if (spineAndManifest.first.empty() || spineAndManifest.second.empty()) {
        std::cerr << "Failed to extract spine and manifest from the OPF file." << "\n";
        return 1;
    }

    std::cout << "After extractSpineAndManifest" << "\n";

    // extract ids from the manifest
    std::vector<std::pair<std::string, std::string>> manifestMappingIds = extractManifestIds(spineAndManifest.first);

    if (manifestMappingIds.empty()) {
        std::cerr << "Failed to extract manifest ids." << "\n";
        return 1;
    }

    std::cout << "After extractManifestIds" << "\n";

    

    std::vector<std::filesystem::path> xhtmlFiles = getAllXHTMLFiles(std::filesystem::path(unzippedPath));
    if (xhtmlFiles.empty()) {
        std::cerr << "No XHTML files found in the unzipped EPUB directory." << "\n";
        return 1;
    }

    // Print the XHTML files
    std::cout << "XHTML Files:" << "\n";
    for (const auto& item : xhtmlFiles) {
        std::cout << item << "\n";
    }

    std::cout << "After getAllXHTMLFiles" << "\n";

    // Sort the XHTML files based on the spine order
    std::vector<std::filesystem::path> spineOrderXHTMLFiles = sortXHTMLFilesBySpineOrder(xhtmlFiles, spineOrder, manifestMappingIds);
    if (spineOrderXHTMLFiles.empty()) {
        std::cerr << "No XHTML files found in the unzipped EPUB directory matching the spine order." << "\n";
        return 1;
    }

    // Print the sorted XHTML files
    std::cout << "Sorted XHTML Files:" << "\n";
    for (const auto& item : spineOrderXHTMLFiles) {
        std::cout << item << "\n";
    }

    std::cout << "After sortXHTMLFilesBySpineOrder" << "\n";

    // Duplicate Section001.xhtml for each xhtml file in spineOrderXHTMLFiles and rename it
    std::filesystem::path Section001Path = std::filesystem::path("export/OEBPS/Text/Section0001.xhtml");
    std::ifstream Section001File(Section001Path);
    if (!Section001File.is_open()) {
        std::cerr << "Failed to open Section001.xhtml file: " << Section001Path << "\n";
        return 1;
    }

    std::string Section001Content((std::istreambuf_iterator<char>(Section001File)), std::istreambuf_iterator<char>());
    Section001File.close();

    for (size_t i = 0; i < spineOrderXHTMLFiles.size(); ++i) {
        std::filesystem::path newSectionPath = std::filesystem::path("export/OEBPS/Text/" +  spineOrderXHTMLFiles[i].filename().string());
        std::ofstream newSectionFile(newSectionPath);
        if (!newSectionFile.is_open()) {
            std::cerr << "Failed to create new Section" << i + 1 << ".xhtml file." << "\n";
            return 1;
        }
        newSectionFile << Section001Content;
        newSectionFile.close();
    }
    //Remove Section001.xhtml
    std::filesystem::remove(Section001Path);

    std::cout << "After duplicate Section001.xhtml" << "\n";

    // Update the spine and manifest in the templates OPF file
    std::filesystem::path templateContentOpfPath = "export/OEBPS/content.opf";

    updateContentOpf(spineOrder, templateContentOpfPath, manifestMappingIds);

    std::cout << "After updateContentOpf" << "\n";

    // Update the nav.xhtml file
    std::filesystem::path navXHTMLPath = "export/OEBPS/Text/nav.xhtml";
    updateNavXHTML(navXHTMLPath, spineOrder);

    std::cout << "After updateNavXHTML" << "\n";

    // Copy images from the unzipped directory to the template directory
    copyImages(std::filesystem::path(unzippedPath), std::filesystem::path("export/OEBPS/Images"));


    // check if book_details.txt exists
    std::filesystem::path bookDetailsPath = "book_details.txt";

    if (std::filesystem::exists(bookDetailsPath)) {
        std::ifstream bookDetailsFile(bookDetailsPath);
        if (!bookDetailsFile.is_open()) {
            std::cerr << "Failed to open book_details.txt file." << "\n";
            return 1;
        }

        std::string title;
        std::string author;
        std::getline(bookDetailsFile, title);
        std::getline(bookDetailsFile, author);
        bookDetailsFile.close();

        std::cout << "Title: " << title << "\n";
        std::cout << "Author: " << author << "\n";

        std::string templateContentOpfPathString = "export/OEBPS/content.opf";

        addTitleAndAuthor(templateContentOpfPathString.c_str(), title, author);
    }

    // Clean each chapter
    for (const auto& xhtmlFile : spineOrderXHTMLFiles) {
            cleanChapter(xhtmlFile);
            std::cout << "Chapter cleaned: " << xhtmlFile.string() << "\n";
    }

    //Extract all of the relevant tags
    std::vector<tagData> bookTags = extractTags(spineOrderXHTMLFiles);

    if (bookTags.empty()) {
        std::cerr << "No tags extracted from the book." << "\n";
        return 1;
    }

    if (localModel == 1){
        if (deepLKey.empty()) {
            std::cerr << "No DeepL API key provided." << "\n";
            return 1;
        }

        int result = handleDeepLRequest(bookTags, spineOrderXHTMLFiles, deepLKey);

        if (result != 0) {
            std::cerr << "Failed to handle DeepL request." << "\n";
            return 1;
        }


        exportEpub(templatePath, outputEpubPath);
        
        // // Remove the unzipped and export directories
        std::filesystem::remove_all(unzippedPath);
        std::filesystem::remove_all(templatePath);
        std::filesystem::remove_all("translatedHTML");
        std::filesystem::remove_all("testHTML");
        std::filesystem::remove("rawTags.txt");
        std::filesystem::remove("translatedTags.txt");

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Time taken: " << elapsed.count() << "s" << "\n";

        return 0;
    }


    std::string rawTagsPathString = "rawTags.txt";
    std::string translatedTagsPathString = "translatedTags.txt";

    // Write out a file of the raw tags
    std::filesystem::path rawTagsPath = rawTagsPathString;
    std::ofstream rawTagsFile(rawTagsPath);
    if (!rawTagsFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << rawTagsPath << "\n";
        return 1;
    }

    for (auto& tag : bookTags) {
        if (tag.tagId == IMG_TAG) {
            rawTagsFile <<  tag.tagId << "," << tag.chapterNum << "," << tag.position << "," << tag.text << "\n";
        } else if (tag.tagId == P_TAG) {
            rawTagsFile <<  tag.tagId << "," << tag.chapterNum << "," << tag.position << "," << ">>" << langcode << "<< " << tag.text << "\n";
        }
    }
    rawTagsFile.close();
    
    std::string chapterNumberMode = "0";
    
    //Start the multiprocessing translaton
    std::filesystem::path translationExe;

    #if defined(__APPLE__)
        translationExe = currentDirPath / "translation";

    #elif defined(_WIN32)
        translationExe = currentDirPath / "translation.exe";
    #else
        std::cerr << "Unsupported platform!" << std::endl;
        return 1; // Or some other error handling
    #endif

    if (!std::filesystem::exists(translationExe)) {
        std::cerr << "Executable not found: " << translationExe << std::endl;
        return 1;
    }

    std::string translationExePath = translationExe.string();
    

    std::cout << "Before call to translation.exe" << '\n';

    boost::process::ipstream pipe_stdout, pipe_stderr;

    try {

        boost::process::child c(
            translationExePath,
            rawTagsPathString,
            chapterNumberMode,
            boost::process::std_out > pipe_stdout, 
            boost::process::std_err > pipe_stderr
        );

        // Threads to handle asynchronous reading
        std::thread stdout_thread([&pipe_stdout]() {
            std::string line;
            while (std::getline(pipe_stdout, line)) {
                std::cout << "Python stdout: " << line << "\n";
            }
        });

        std::thread stderr_thread([&pipe_stderr]() {
            std::string line;
            while (std::getline(pipe_stderr, line)) {
                std::cerr << "Python stderr: " << line << "\n";
            }
        });

        c.wait(); // Wait for process completion

        stdout_thread.join();
        stderr_thread.join();

        if (c.exit_code() == 0) {
            std::cout << "Translation Python script executed successfully." << "\n";
        } else {
            std::cerr << "Translation Python script exited with code: " << c.exit_code() << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << "\n";
    }

    std::cout << "After call to translation.exe" << '\n';

    std::vector<decodedData> decodedDataVector;
    std::ifstream file(translatedTagsPathString);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << translatedTagsPathString << "\n";
        return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream lineStream(line);
        std::string chapterNumStr, positionStr, text;

        // Extract chapterNum (first value)
        if (!std::getline(lineStream, chapterNumStr, ',')) continue;

        // Extract position (second value)
        if (!std::getline(lineStream, positionStr, ',')) continue;

        // The rest of the line is text
        std::getline(lineStream, text);

        // Convert chapterNum and position to integers
        try {
            decodedData data;
            data.chapterNum = std::stoi(chapterNumStr);
            data.position = std::stoi(positionStr);
            data.output = text;
            // Store the extracted data
            decodedDataVector.push_back(data);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << " - " << e.what() << "\n";
        }
    }
    file.close();




    std::vector<std::vector<tagData>> chapterTags;
    // Divide bookTags into chapters chapterNum is the chapter number
    for (auto& tag : bookTags) {
        if (tag.chapterNum >= chapterTags.size()) {
            chapterTags.resize(tag.chapterNum + 1);
        }
        chapterTags[tag.chapterNum].push_back(tag);
    }

    // Build the position map for each chapter and position
    std::unordered_map<int, std::unordered_map<int, tagData*>> positionMap;

    for (size_t chapterNum = 0; chapterNum < chapterTags.size(); ++chapterNum) {
        for (auto& tag : chapterTags[chapterNum]) {
            positionMap[chapterNum][tag.position] = &tag;
        }
    }

    // Update chapterTags using decodedDataVector
    for (const auto& decoded : decodedDataVector) {
        // Find the corresponding chapter in the map
        auto chapterIt = positionMap.find(decoded.chapterNum);
        if (chapterIt != positionMap.end()) {
            auto& positionTags = chapterIt->second;
            // Find the tag by position
            auto tagIt = positionTags.find(decoded.position);
            if (tagIt != positionTags.end()) {
                // Update the text of the corresponding tag
                tagIt->second->text = decoded.output;
                // std::cout << "Decoded text: " << decoded.output << "\n";
            }
        }
    }

    //Write out to the template EPUB
    std::string htmlHeader = R"(<?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE html>
    <html xmlns="http://www.w3.org/1999/xhtml">
    <head>
    <title>)";

    std::string htmlFooter = R"(</body>
    </html>)";

    for (size_t i = 0; i < spineOrderXHTMLFiles.size(); ++i) {
        std::filesystem::path outputPath = "export/OEBPS/Text/" + spineOrderXHTMLFiles[i].filename().string();
        std::ofstream outFile(outputPath);
        std::cout << "Writing to: " << outputPath << "\n";
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << outputPath << "\n";
            return 1;
        }

        // Write pre-built header
        outFile << htmlHeader << spineOrderXHTMLFiles[i].filename().string() << "</title>\n</head>\n<body>\n";

        if (i > chapterTags.size()) {
            outFile << htmlFooter;
            outFile.close();
            continue;
        }

        // Write content-specific parts
        for (const auto& tag : chapterTags[i]) {
            if (tag.tagId == P_TAG) {
                outFile << "<p>" << tag.text << "</p>\n";
            } else if (tag.tagId == IMG_TAG) {
                outFile << "<img src=\"../Images/" << tag.text << "\" alt=\"\"/>\n";
            }
        }

        // Write pre-built footer
        outFile << htmlFooter;
        outFile.close();
    }

    // Zip export directory to create the final EPUB file
    exportEpub(templatePath, outputEpubPath);

    // // Remove the unzipped and export directories
    std::filesystem::remove_all(unzippedPath);
    std::filesystem::remove_all(templatePath);


    // // Remove the temp text files
    try {
        if (std::filesystem::exists(translatedTagsPathString)) {
            std::filesystem::remove(translatedTagsPathString);
            std::cout << "Deleted file: " << translatedTagsPathString << "\n";
        }
        if (std::filesystem::exists(rawTagsPathString)) {
            std::filesystem::remove(rawTagsPathString);
            std::cout << "Deleted file: " << rawTagsPathString << "\n";
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
    }

    // End timer
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Time taken: " << elapsed.count() << "s" << "\n";

    return 0;
}
