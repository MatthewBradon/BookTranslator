#include "DocxTranslator.h"


int DocxTranslator::run(const std::string& inputPath, const std::string& outputPath, int localModel, const std::string& deepLKey) {

    // Unzip the DOCX file
    std::string unzippedPath = "unzipped";

    // Check if the unzipped directory already exists
    if (std::filesystem::exists(unzippedPath)) {
        std::cout << "Unzipped directory already exists. Deleting it..." << "\n";
        std::filesystem::remove_all(unzippedPath);
    }

    // Start the timer
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "START" << "\n";

    // Create the output directory if it doesn't exist
    if (!make_directory(unzippedPath)) {
        std::cerr << "Failed to create output directory: " << unzippedPath << "\n";
        return 1;
    }

    // Unzip the DOCX file
    if (!unzip_file(inputPath, unzippedPath)) {
        std::cerr << "Failed to unzip DOCX file: " << inputPath << "\n";
        return 1;
    }

    std::cout << "DOCX file unzipped successfully to: " << unzippedPath << "\n";

    // Get the path to the document.xml file
    std::string documentXmlPath = unzippedPath + "/word/document.xml";

    // Check if the document.xml file exists
    if (!std::filesystem::exists(documentXmlPath)) {
        std::cerr << "document.xml file not found in DOCX archive." << "\n";
        return 1;
    }

    // Parse the document.xml file
    xmlDocPtr doc = xmlReadFile(documentXmlPath.c_str(), NULL, 0);

    if (doc == NULL) {
        std::cerr << "Failed to parse document.xml file." << "\n";
        return 1;
    }

    // Get the root element of the XML document
    xmlNodePtr root = xmlDocGetRootElement(doc);

    // Extract text nodes from the XML document
    std::vector<TextNode> textNodes = extractTextNodes(root);

    // Save the extracted text to a file
    std::string textFilePath = "extracted_text.txt";
    std::string positionFilePath = "position_tags.txt";

    saveTextToFile(textNodes, positionFilePath, textFilePath);


    
    std::cout << "Before call to tokenizeRawTags.exe" << '\n';
    boost::filesystem::path exePath;
    #if defined(__APPLE__)
        exePath = "tokenizeRawTags";

    #elif defined(_WIN32)
        exePath = "tokenizeRawTags.exe";
    #else
        std::cerr << "Unsupported platform!" << std::endl;
        return 1; // Or some other error handling
    #endif
    boost::filesystem::path inputFilePath = textFilePath;  // Path to the input file
    std::string chapterNumberMode = "1";
    // Ensure the .exe exists
    if (!boost::filesystem::exists(exePath)) {
        std::cerr << "Executable not found: " << exePath << std::endl;
        return 1;
    }

    // Ensure the input file exists
    if (!boost::filesystem::exists(inputFilePath)) {
        std::cerr << "Input file not found: " << inputFilePath << std::endl;
        return 1;
    }

    // Create pipes for capturing stdout and stderr
    boost::process::ipstream encodeTagspipe_stdout;
    boost::process::ipstream encodeTagspipe_stderr;

    try {
        // Start the .exe process with arguments
        boost::process::child c(
            exePath.string(),                 // Executable path
            inputFilePath.string(),           // Argument: path to input file
            chapterNumberMode,                // Argument: chapter number mode
            boost::process::std_out > encodeTagspipe_stdout,        // Redirect stdout
            boost::process::std_err > encodeTagspipe_stderr         // Redirect stderr
        );

        // Read stdout
        std::string line;
        while (c.running() && std::getline(encodeTagspipe_stdout, line)) {
            std::cout << line << std::endl;
        }

        // Read any remaining stdout
        while (std::getline(encodeTagspipe_stdout, line)) {
            std::cout << line << std::endl;
        }

        // Read stderr
        while (std::getline(encodeTagspipe_stderr, line)) {
            std::cerr << "Error: " << line << std::endl;
        }

        // Wait for the process to exit
        c.wait();

        // Check the exit code
        if (c.exit_code() == 0) {
            std::cout << "tokenizeRawTags.exe executed successfully." << std::endl;
        } else {
            std::cerr << "tokenizeRawTags.exe exited with code: " << c.exit_code() << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "After call to tokenizeRawTags.exe" << '\n';


    //Start the multiprocessing translaton
    std::filesystem::path multiprocessExe;

    #if defined(__APPLE__)
        multiprocessExe = "multiprocessTranslation";

    #elif defined(_WIN32)
        multiprocessExe = "multiprocessTranslation.exe";
    #else
        std::cerr << "Unsupported platform!" << std::endl;
        return 1; // Or some other error handling
    #endif

    if (!std::filesystem::exists(multiprocessExe)) {
        std::cerr << "Executable not found: " << multiprocessExe << std::endl;
        return 1;
    }

    std::string multiprocessExePath = multiprocessExe.string();
    

    std::cout << "Before call to multiprocessTranslation.py" << '\n';

    boost::process::ipstream pipe_stdout, pipe_stderr;

    try {

        boost::process::child c(
            multiprocessExePath,
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

    std::cout << "After call to multiprocessTranslation.py" << '\n';

    // Load the translations
    std::unordered_multimap<std::string, std::string> translations = loadTranslations("position_tags.txt", "translatedTags.txt");


    // Print the translations
    for (const auto& [path, text] : translations) {
        std::cout << path << " -> " << text << "\n";
    }
    try{
        // Reinsert the translations into the XML document
        reinsertTranslations(root, translations);
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << "\n";
    }

    // Save the modified XML document to a file
    if (xmlSaveFileEnc(documentXmlPath.c_str(), doc, "UTF-8") == -1) {
        std::cerr << "Failed to save modified XML document." << "\n";
        return 1;
    }

    // Free the XML document
    xmlFreeDoc(doc);

    std::cout << "Modified XML document saved to: " << documentXmlPath << "\n";

    exportDocx(unzippedPath, outputPath);

    
    // End timer
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Time taken: " << elapsed.count() << "s" << "\n";

    return 0;
}

bool DocxTranslator::make_directory(const std::filesystem::path& path) {
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

bool DocxTranslator::unzip_file(const std::string& zipPath, const std::string& outputDir) {
    int err = 0;
    zip* archive = zip_open(zipPath.c_str(), ZIP_RDONLY, &err);
    if (archive == nullptr) {
        std::cerr << "Error opening ZIP archive: " << zipPath << "\n";
        return false;
    }

    // Get the number of entries (files) in the archive
    zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    for (zip_uint64_t i = 0; i < numEntries; ++i) {
        // Get the file name
        const char* name = zip_get_name(archive, i, 0);
        if (name == nullptr) {
            std::cerr << "Error getting file name in ZIP archive." << "\n";
            zip_close(archive);
            return false;
        }

        // Create the full output path for this file
        std::filesystem::path outputPath = std::filesystem::path(outputDir) / name;

        // Check if the file is in a directory (ends with '/')
        if (outputPath.filename().string().back() == '/') {
            // Create directory
            make_directory(outputPath);
        } else {
            // Ensure the directory for the output file exists
            make_directory(outputPath.parent_path());

            // Open the file inside the ZIP archive
            zip_file* file = zip_fopen_index(archive, i, 0);
            if (file == nullptr) {
                std::cerr << "Error opening file in ZIP archive: " << name << "\n";
                zip_close(archive);
                return false;
            }

            // Open the output file
            std::ofstream outputFile(outputPath, std::ios::binary);
            if (!outputFile.is_open()) {
                std::cerr << "Error creating output file: " << outputPath << "\n";
                zip_fclose(file);
                zip_close(archive);
                return false;
            }

            // Read from the ZIP file and write to the output file
            char buffer[4096];
            zip_int64_t bytesRead;
            while ((bytesRead = zip_fread(file, buffer, sizeof(buffer))) > 0) {
                outputFile.write(buffer, bytesRead);
            }

            outputFile.close();

            // Close the current file in the ZIP archive
            zip_fclose(file);
        }
    }

    // Close the ZIP archive
    zip_close(archive);
    return true;
}

// Helper function to get XPath-like node path
std::string DocxTranslator::getNodePath(xmlNode *node) {
    std::string path;
    while (node) {
        if (node->name) {
            path = "/" + std::string((const char*)node->name) + path;
        }
        node = node->parent;
    }
    return path;
}

// Optimized Step 1: Extract text from <w:t> nodes and record positions
void DocxTranslator::extractTextNodesRecursive(xmlNode *node, std::vector<TextNode> &nodes) {
    for (; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, BAD_CAST "t") == 0) {
            xmlChar *content = xmlNodeGetContent(node);
            if (content && xmlStrlen(content) > 0) {
                nodes.push_back({getNodePath(node), (const char*)content});
                xmlFree(content);
            }
        }
        extractTextNodesRecursive(node->children, nodes);
    }
}

// Wrapper function to start the extraction
std::vector<TextNode> DocxTranslator::extractTextNodes(xmlNode *root) {
    std::vector<TextNode> nodes;
    nodes.reserve(10000);  // Reserve large space to avoid multiple allocations
    extractTextNodesRecursive(root, nodes);
    return nodes;
}


// Step 2: Save extracted text to a file
// Step 2: Save extracted text and positions to separate files
void DocxTranslator::saveTextToFile(const std::vector<TextNode> &nodes, const std::string &positionFilename, const std::string &textFilename) {
    std::ofstream positionFile(positionFilename);
    std::ofstream textFile(textFilename);

    if (!positionFile.is_open() || !textFile.is_open()) {
        std::cerr << "Error opening output files.\n";
        return;
    }

    int counterNum = 1;
    for (const auto &node : nodes) {
        // Write to positionTags.txt: "counterNum,node.path"
        positionFile << counterNum << "," << node.path << "\n";
        // Write to extracted_text.txt: "counterNum,node.text"
        textFile << counterNum << "," << node.text << "\n";
        ++counterNum;
    }

    positionFile.close();
    textFile.close();

    std::cout << "Positions saved to " << positionFilename << std::endl;
    std::cout << "Texts saved to " << textFilename << std::endl;
}


std::unordered_multimap<std::string, std::string> DocxTranslator::loadTranslations(const std::string &positionFilename, const std::string &textFilename) {

    std::unordered_multimap<std::string, std::string> translations;

    std::ifstream positionFile(positionFilename);
    std::ifstream textFile(textFilename);

    if (!positionFile.is_open() || !textFile.is_open()) {
        std::cerr << "Error opening translation files.\n";
        return translations;
    }

    std::string positionLine, textLine;

    while (std::getline(positionFile, positionLine) && std::getline(textFile, textLine)) {
        // Parse position line: "counterNum,node.path"
        size_t posDelimiter = positionLine.find(',');
        if (posDelimiter == std::string::npos) continue;

        std::string posCounter = positionLine.substr(0, posDelimiter);
        std::string nodePath = positionLine.substr(posDelimiter + 1);

        // Parse text line: "counterNum,node.text"
        size_t textDelimiter = textLine.find(',');
        if (textDelimiter == std::string::npos) continue;

        std::string textCounter = textLine.substr(0, textDelimiter);
        std::string nodeText = textLine.substr(textDelimiter + 1);

        // Match counterNum before inserting
        if (posCounter == textCounter) {
            translations.insert({nodePath, nodeText});
        } else {
            std::cerr << "Mismatched counters: Position " << posCounter 
                      << " vs Text " << textCounter << "\n";
        }
    }

    positionFile.close();
    textFile.close();

    std::cout << "Loaded " << translations.size() << " translations.\n";
    return translations;
}

// Helper function to update text content and language attribute
void DocxTranslator::updateNodeWithTranslation(xmlNode *node, const std::string &translation) {
    // Replace text content
    xmlNodeSetContent(node, BAD_CAST translation.c_str());

    // Update parent's language attribute to English
    if (xmlNode *parent = node->parent) {
        xmlNewProp(parent, BAD_CAST "xml:lang", BAD_CAST "en-US");
    }
}

// Recursive function to traverse nodes and insert translations
void DocxTranslator::traverseAndReinsert(xmlNode *node, std::unordered_multimap<std::string, std::string> &translations, std::unordered_map<std::string, std::unordered_multimap<std::string, std::string>::iterator> &lastUsed) {
    for (; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, BAD_CAST "t") == 0) {
            std::string path = getNodePath(node);

            // Find available translations for this path
            auto range = translations.equal_range(path);
            if (range.first != range.second) {
                // Initialize iterator if not set
                if (lastUsed.find(path) == lastUsed.end()) {
                    lastUsed[path] = range.first;
                } else {
                    // Increment and loop if needed
                    auto &it = lastUsed[path];
                    ++it;
                    if (it == range.second) {
                        it = range.first; // Cycle back to the first translation
                    }
                }

                // Insert the translation
                updateNodeWithTranslation(node, lastUsed[path]->second);
            }
        }
        // Recurse into child nodes
        traverseAndReinsert(node->children, translations, lastUsed);
    }
}

// Main function to reinsert translations using multimap
void DocxTranslator::reinsertTranslations(xmlNode *root, std::unordered_multimap<std::string, std::string> &translations) {
    if (!root) {
        std::cerr << "Error: XML root node is null.\n";
        return;
    }

    // Track which translation was last used for each node path
    std::unordered_map<std::string, std::unordered_multimap<std::string, std::string>::iterator> lastUsed;

    // Begin recursive traversal and reinsertion
    try {
        traverseAndReinsert(root, translations, lastUsed);
    } catch (const std::exception &e) {
        std::cerr << "Exception during reinsert: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "Unknown error during reinsert.\n";
    }
}

void DocxTranslator::exportDocx(const std::string& exportPath, const std::string& outputDir) {
    // Check if the exportPath directory exists
    if (!std::filesystem::exists(exportPath)) {
        std::cerr << "Export directory does not exist: " << exportPath << "\n";
        return;
    }

    // Create the output DOCX file path
    std::string docxPath = outputDir + "/output.docx";

    // Create a ZIP archive for the DOCX
    zip_t* archive = zip_open(docxPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, nullptr);
    if (!archive) {
        std::cerr << "Error creating ZIP archive: " << docxPath << "\n";
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

            // Add the file to the ZIP archive with UTF-8 encoding
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
        std::cerr << "Error closing ZIP archive: " << docxPath << "\n";
        return;
    }

    std::cout << "DOCX file created: " << docxPath << "\n";
}
