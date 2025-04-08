#include "HTMLTranslator.h"

int HTMLTranslator::run(const std::string& inputPath, const std::string& outputPath, int localModel, const std::string& deepLKey, std::string langcode) {
    // Verify that the input file exists.
    std::filesystem::path inputHtmlPath = std::filesystem::u8path(inputPath);
    if (!std::filesystem::exists(inputHtmlPath)) {
        std::cerr << "Input file does not exist: " << inputHtmlPath.string() << std::endl;
        return 1;
    }

    // Start timer
    auto start = std::chrono::high_resolution_clock::now();

    // Parse the HTML file
    htmlDocPtr doc = htmlReadFile(inputPath.c_str(), nullptr, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (doc == nullptr) {
        std::cerr << "Failed to parse HTML file: " << inputPath << std::endl;
        return 1;
    }

    // Get the root element
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) {
        std::cerr << "HTML document is empty: " << inputPath << std::endl;
        xmlFreeDoc(doc);
        return 1;
    }

    // Extract text nodes
    std::vector<TextNode> textNodes = extractTextNodes(root);

    // Save extracted texts and their positions
    std::string textFilePath = "extracted_text.txt";
    std::string positionFilePath = "position_tags.txt";
    saveTextToFile(textNodes, positionFilePath, textFilePath, langcode);

    // Call an external translation executable
    std::filesystem::path translationExe;
#if defined(__APPLE__)
    translationExe = "translation";
#elif defined(_WIN32)
    translationExe = "translation.exe";
#else
    std::cerr << "Unsupported platform!" << std::endl;
    return 1;
#endif

    if (!std::filesystem::exists(translationExe)) {
        std::cerr << "Executable not found: " << translationExe.string() << std::endl;
        return 1;
    }

    std::string translationExePath = translationExe.string();
    std::cout << "Before call to translation executable" << std::endl;

    boost::process::ipstream pipe_stdout, pipe_stderr;
    try {
#if defined(_WIN32)
        boost::process::child c(
            translationExePath,
            textFilePath,
            "1", // chapter_num_mode
            boost::process::std_out > pipe_stdout,
            boost::process::std_err > pipe_stderr,
            boost::process::windows::hide
        );
#else
        boost::process::child c(
            translationExePath,
            textFilePath,
            "1", // chapter_num_mode
            boost::process::std_out > pipe_stdout,
            boost::process::std_err > pipe_stderr
        );
#endif

        std::thread stdout_thread([&pipe_stdout]() {
            std::string line;
            while (std::getline(pipe_stdout, line))
                std::cout << line << std::endl;
        });
        std::thread stderr_thread([&pipe_stderr]() {
            std::string line;
            while (std::getline(pipe_stderr, line))
                std::cerr << "Translation stderr: " << line << std::endl;
        });

        c.wait();
        stdout_thread.join();
        stderr_thread.join();

        if (c.exit_code() == 0) {
            std::cout << "Translation executable executed successfully." << std::endl;
        } else {
            std::cerr << "Translation executable exited with code: " << c.exit_code() << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception during translation executable call: " << ex.what() << std::endl;
    }
    std::cout << "After call to translation executable" << std::endl;

    // Load translations produced by the translation process
    std::unordered_multimap<std::string, std::string> translations = loadTranslations(positionFilePath, "translatedTags.txt");
    escapeTranslations(translations);

    // Reinsert translations into the HTML document
    try {
        reinsertTranslations(root, translations);
    } catch (const std::exception& ex) {
        std::cerr << "Exception during reinsertion: " << ex.what() << std::endl;
    }
        
    std::string outputFileName = "output.html";
    std::filesystem::path outputFilePath = std::filesystem::u8path(outputPath) / outputFileName;
    int saveResult = htmlSaveFileEnc(outputFilePath.string().c_str(), doc, "UTF-8");
    if (saveResult < 0) {
        std::cerr << "Failed to save HTML document: " << outputFilePath.string() << std::endl;
        xmlFreeDoc(doc);
        return 1;
    }
    
    std::cout << "Modified HTML document saved to: " << outputFilePath << std::endl;

    // Cleanup temporary files
    std::filesystem::remove(positionFilePath);
    std::filesystem::remove(textFilePath);
    std::filesystem::remove("translatedTags.txt");

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Time taken: " << elapsed.count() << " seconds" << std::endl;

    return 0;
}


std::vector<TextNode> HTMLTranslator::extractTextNodes(xmlNode* root) {
    std::vector<TextNode> nodes;
    nodes.reserve(10000);
    extractTextNodesRecursive(root, nodes);
    return nodes;
}

void HTMLTranslator::extractTextNodesRecursive(xmlNode* node, std::vector<TextNode>& nodes) {
    for (; node; node = node->next) {
        if (node->type == XML_TEXT_NODE) {
            xmlChar* content = xmlNodeGetContent(node);
            if (content && xmlStrlen(content) > 0) {
                std::string text(reinterpret_cast<const char*>(content));
                // Skip text that is only whitespace
                if (text.find_first_not_of(" \t\n\r") != std::string::npos) {
                    nodes.push_back({ getNodePath(node), text });
                }
                xmlFree(content);
            }
        }
        extractTextNodesRecursive(node->children, nodes);
    }
}

std::string HTMLTranslator::getNodePath(xmlNode* node) {
    std::string path;
    while (node) {
        if (node->name) {
            path = "/" + std::string(reinterpret_cast<const char*>(node->name)) + path;
        }
        node = node->parent;
    }
    return path;
}


void HTMLTranslator::traverseAndReinsert(xmlNode* node, std::unordered_multimap<std::string, std::string>& translations, std::unordered_map<std::string, std::unordered_multimap<std::string, std::string>::iterator>& lastUsed)
{
    for (; node; node = node->next) {
        if (node->type == XML_TEXT_NODE) {
            std::string path = getNodePath(node);
            auto range = translations.equal_range(path);
            if (range.first != range.second) {
                if (lastUsed.find(path) == lastUsed.end()) {
                    lastUsed[path] = range.first;
                } else {
                    auto& it = lastUsed[path];
                    ++it;
                    if (it == range.second)
                        it = range.first;
                }
                xmlNodeSetContent(node, BAD_CAST lastUsed[path]->second.c_str());
            }
        }
        traverseAndReinsert(node->children, translations, lastUsed);
    }
}

void HTMLTranslator::reinsertTranslations(xmlNode* root, std::unordered_multimap<std::string, std::string>& translations) {
    if (!root) {
        std::cerr << "Error: HTML root node is null." << std::endl;
        return;
    }
    std::unordered_map<std::string, std::unordered_multimap<std::string, std::string>::iterator> lastUsed;
    try {
        traverseAndReinsert(root, translations, lastUsed);
    } catch (const std::exception& e) {
        std::cerr << "Exception during reinsertion: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error during reinsertion." << std::endl;
    }
}


void HTMLTranslator::saveTextToFile(const std::vector<TextNode>& nodes, const std::string& positionFilename, const std::string& textFilename, const std::string& langcode) {
    std::ofstream positionFile(positionFilename);
    std::ofstream textFile(textFilename);
    if (!positionFile.is_open() || !textFile.is_open()) {
        std::cerr << "Error opening output files." << std::endl;
        return;
    }
    int counterNum = 1;
    for (const auto& node : nodes) {
        // Save the node path along with a counter
        positionFile << counterNum << "," << node.path << "\n";
        // Save the text with the language code tag
        textFile << counterNum << ",>>" << langcode << "<< " << node.text << "\n";
        ++counterNum;
    }
    positionFile.close();
    textFile.close();
    std::cout << "Positions saved to " << positionFilename << std::endl;
    std::cout << "Texts saved to " << textFilename << std::endl;
}

std::unordered_multimap<std::string, std::string> HTMLTranslator::loadTranslations(const std::string& positionFilename, const std::string& textFilename) {
    std::unordered_multimap<std::string, std::string> translations;
    std::ifstream positionFile(positionFilename);
    std::ifstream textFile(textFilename);
    if (!positionFile.is_open() || !textFile.is_open()) {
        std::cerr << "Error opening translation files." << std::endl;
        return translations;
    }
    std::string positionLine, textLine;
    while (std::getline(positionFile, positionLine) && std::getline(textFile, textLine)) {
        size_t posDelimiter = positionLine.find(',');
        if (posDelimiter == std::string::npos) continue;
        std::string posCounter = positionLine.substr(0, posDelimiter);
        std::string nodePath = positionLine.substr(posDelimiter + 1);
        size_t textDelimiter = textLine.find(',');
        if (textDelimiter == std::string::npos) continue;
        std::string textCounter = textLine.substr(0, textDelimiter);
        std::string nodeText = textLine.substr(textDelimiter + 1);
        if (posCounter == textCounter) {
            translations.insert({ nodePath, nodeText });
        } else {
            std::cerr << "Mismatched counters: Position " << posCounter
                      << " vs Text " << textCounter << std::endl;
        }
    }
    positionFile.close();
    textFile.close();
    std::cout << "Loaded " << translations.size() << " translations." << std::endl;
    return translations;
}

std::string HTMLTranslator::escapeForHtml(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '&':  escaped.append("&amp;");  break;
            case '<':  escaped.append("&lt;");   break;
            case '>':  escaped.append("&gt;");   break;
            case '"':  escaped.append("&quot;"); break;
            case '\'': escaped.append("&apos;"); break;
            default:   escaped.push_back(ch);    break;
        }
    }
    return escaped;
}

void HTMLTranslator::escapeTranslations(std::unordered_multimap<std::string, std::string>& translations) {
    for (auto& pair : translations) {
        pair.second = escapeForHtml(pair.second);
    }
}
