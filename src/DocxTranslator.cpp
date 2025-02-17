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


// Step 3: Read translations from a file
std::unordered_map<std::string, std::string> DocxTranslator::loadTranslations(const std::string &filename) {
    std::unordered_map<std::string, std::string> translations;
    std::ifstream infile(filename);
    std::string line;
    while (std::getline(infile, line)) {
        size_t delimiter = line.find("||");
        if (delimiter != std::string::npos) {
            std::string path = line.substr(0, delimiter);
            std::string translation = line.substr(delimiter + 2);
            translations[path] = translation;
        }
    }
    infile.close();
    return translations;
}

// Step 4: Reinsert translations into the XML
void DocxTranslator::reinsertTranslations(xmlNode *root, const std::unordered_map<std::string, std::string> &translations) {
    for (xmlNode *node = root; node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, BAD_CAST "t") == 0) {
            std::string path = getNodePath(node);
            if (translations.find(path) != translations.end()) {
                // Replace text content
                xmlNodeSetContent(node, BAD_CAST translations.at(path).c_str());

                // Update language attribute to English
                xmlNode *parent = node->parent;
                if (parent) {
                    xmlNewProp(parent, BAD_CAST "xml:lang", BAD_CAST "en-US");
                }
            }
        }
        reinsertTranslations(node->children, translations);
    }
}


