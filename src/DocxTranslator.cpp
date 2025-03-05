#include "DocxTranslator.h"


int DocxTranslator::run(const std::string& inputPath, const std::string& outputPath, int localModel, const std::string& deepLKey, std::string langcode) {

    // Check if the input file exists
    std::filesystem::path inputDocxPath = std::filesystem::u8path(inputPath);

    if (!std::filesystem::exists(inputDocxPath)) {
        std::cerr << "Input file does not exist: " << inputDocxPath.string() << std::endl;
        return 1;
    }

    if (localModel == 1) {

        std::filesystem::path outputDir = outputPath;

        outputDir = outputDir / "output.docx";

        std::string outputPathString = outputDir.string();

        std::cout << "outputPath: " << outputPathString << "\n";

        int result = handleDeepLRequest(inputPath, outputPathString, deepLKey);


        if (result != 0) {
            std::cerr << "Failed to handle DeepL request." << std::endl;
            return 1;
        }

        return 0;
    }


    // Unzip the DOCX file
    std::string unzippedPath = "unzipped";

    std::filesystem::path unzipppedPathU8 = std::filesystem::u8path(unzippedPath);

    // Check if the unzipped directory already exists
    if (std::filesystem::exists(unzipppedPathU8)) {
        std::cout << "Unzipped directory already exists. Deleting it..." << "\n";
        std::filesystem::remove_all(unzipppedPathU8);
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

    std::filesystem::path documentXmlPathU8 = std::filesystem::u8path(documentXmlPath);

    // Check if the document.xml file exists
    if (!std::filesystem::exists(documentXmlPathU8)) {
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

    saveTextToFile(textNodes, positionFilePath, textFilePath, langcode);

    std::string chapterNumberMode = "1";
    
    //Start the multiprocessing translaton
    std::filesystem::path translationExe;

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
    

    std::cout << "Before call to translation.exe" << '\n';

    boost::process::ipstream pipe_stdout, pipe_stderr;

    try {
        #if defined(_WIN32)
            boost::process::child c(
                translationExePath,
                textFilePath,
                chapterNumberMode,
                boost::process::std_out > pipe_stdout, 
                boost::process::std_err > pipe_stderr,
                boost::process::windows::hide
            );
        #else
            boost::process::child c(
                translationExePath,
                textFilePath,
                chapterNumberMode,
                boost::process::std_out > pipe_stdout, 
                boost::process::std_err > pipe_stderr
            );
        #endif

        // Threads to handle asynchronous reading
        std::thread stdout_thread([&pipe_stdout]() {
            std::string line;
            while (std::getline(pipe_stdout, line)) {
                std::cout << line << "\n";
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

    // Load the translations
    std::unordered_multimap<std::string, std::string> translations = loadTranslations(positionFilePath, "translatedTags.txt");
    
    escapeTranslations(translations);

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

    // // Cleanup
    std::filesystem::remove_all(unzippedPath);
    std::filesystem::remove(positionFilePath);
    std::filesystem::remove(textFilePath);
    std::filesystem::remove("translatedTags.txt");

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

// Wrapper function to start the extraction (Needs namespace in the XML)
std::vector<TextNode> DocxTranslator::extractTextNodes(xmlNode *root) {
    std::vector<TextNode> nodes;
    nodes.reserve(10000);  // Reserve large space to avoid multiple allocations
    extractTextNodesRecursive(root, nodes);
    return nodes;
}


void DocxTranslator::saveTextToFile(const std::vector<TextNode> &nodes, const std::string &positionFilename, const std::string &textFilename, const std::string &langcode) {
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
        textFile << counterNum << ",>>" << langcode << "<< " << node.text << "\n";
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

    if (!std::filesystem::exists(outputDir)) {
        std::cerr << "Output path does not exist: " << outputDir << "\n";
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

    // Store created directories to avoid duplicates
    std::unordered_set<std::string> createdDirs;

    // Add all files in the exportPath directory to the ZIP archive
    for (const auto& entry : std::filesystem::recursive_directory_iterator(exportPath)) {
        if (entry.is_regular_file()) {
            std::filesystem::path filePath = entry.path();
            std::filesystem::path relativePath = filePath.lexically_relative(exportPath);

            // Convert to UTF-8 strings and use forward slashes
            std::string filePathUtf8 = filePath.u8string();
            std::string relativePathUtf8 = relativePath.u8string();
            std::replace(relativePathUtf8.begin(), relativePathUtf8.end(), '\\', '/');

            std::cout << "Adding file to DOCX: " << relativePathUtf8 << "\n";

            // Ensure parent directories exist in ZIP archive
            std::filesystem::path parentDir = relativePath.parent_path();
            std::string parentDirUtf8 = parentDir.u8string();
            std::replace(parentDirUtf8.begin(), parentDirUtf8.end(), '\\', '/');

            if (!parentDirUtf8.empty() && createdDirs.find(parentDirUtf8) == createdDirs.end()) {
                zip_dir_add(archive, parentDirUtf8.c_str(), ZIP_FL_ENC_UTF_8);
                createdDirs.insert(parentDirUtf8);
            }

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
        std::cerr << "Error closing ZIP archive: " << docxPath << "\n";
        return;
    }

    std::cout << "DOCX file created: " << docxPath << "\n";
}

std::string DocxTranslator::escapeForDocx(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.size());

    for (char ch : input) {
        switch (ch) {
            case '&':
                escaped.append("&amp;");
                break;
            case '<':
                escaped.append("&lt;");
                break;
            case '>':
                escaped.append("&gt;");
                break;
            case '"':
                escaped.append("&quot;");
                break;
            case '\'':
                escaped.append("&apos;");
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

void DocxTranslator::escapeTranslations(std::unordered_multimap<std::string, std::string>& translations) {
    for (auto& pair : translations) {
        pair.second = escapeForDocx(pair.second);
    }
}


size_t DocxTranslator::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

DocumentInfo DocxTranslator::uploadDocumentToDeepL(const std::string& filePath, const std::string& deepLKey) {
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

    try {
        std::cout << "Document upload response: " << response_string << std::endl;
        nlohmann::json jsonResponse = nlohmann::json::parse(response_string);
        DocumentInfo docInfo;
        docInfo.id = jsonResponse["document_id"];
        docInfo.key = jsonResponse["document_key"];
        return docInfo;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Error parsing document upload response: " << e.what() << std::endl;
        return {};
    }
}

std::string DocxTranslator::checkDocumentStatus(const std::string& document_id, const std::string& document_key, const std::string& deepLKey) {
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

int DocxTranslator::handleDeepLRequest(const std::string& inputPath, const std::string& outputPath, const std::string& deepLKey) {
    // Upload the document to DeepL
    DocumentInfo document = uploadDocumentToDeepL(inputPath, deepLKey);
    if (document.id.empty() || document.key.empty()) {
        std::cerr << "Failed to upload document to DeepL." << std::endl;
        return 1;
    }

    // Poll the status until translation is complete
    while (true) {
        std::string status = checkDocumentStatus(document.id, document.key, deepLKey);
        nlohmann::json jsonResponse = nlohmann::json::parse(status);
        if (jsonResponse["status"] == "done") {
            break;
        }
        std::cout << "Translation in progress... Retrying in 5 seconds." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // Download and save the translated document
    if (!downloadTranslatedDocument(document.id, document.key, deepLKey, outputPath)) {
        std::cerr << "Failed to download translated document." << std::endl;
        return 1;
    }

    std::cout << "Translated document saved to: " << outputPath << std::endl;
    return 0;

}

bool DocxTranslator::downloadTranslatedDocument(const std::string& document_id, const std::string& document_key, const std::string& deepLKey, const std::string& outputPath) {
    CURL* curl;
    CURLcode res;
    std::string api_url = "https://api-free.deepl.com/v2/document/" + document_id + "/result";
    std::string auth_key = "DeepL-Auth-Key " + deepLKey;

    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL." << std::endl;
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: " + auth_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Create JSON payload
    nlohmann::json jsonPayload;
    jsonPayload["document_key"] = document_key;
    std::string json_data = jsonPayload.dump();

    // Open file to write the binary content
    FILE* file = fopen(outputPath.c_str(), "wb");
    if (!file) {
        std::cerr << "Failed to open file for writing: " << outputPath << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        fclose(file);

        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());

    // Write response as binary to file
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

    res = curl_easy_perform(curl);

    fclose(file);  // Ensure file is closed even on failure

    if (res != CURLE_OK) {
        std::cerr << "Curl download request failed: " << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return true;  // Successfully downloaded and saved the document
}
