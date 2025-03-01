#pragma once

#include <zip.h>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <string>
#include <regex>
#include <vector>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <libxml/xmlstring.h>
#include <libxml/encoding.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <cstdlib>
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <sstream>
#include <iostream>
#include <curl/curl.h>
#include "Translator.h"
#include <nlohmann/json.hpp>

struct TextNode {
    std::string path;
    std::string text;
};

struct DocumentInfo {
    std::string id;
    std::string key;
};

class DocxTranslator : public Translator {
public:
    // Implement the run method from the Translator interface
    int run(const std::string& inputPath, const std::string& outputPath, int localModel, const std::string& deepLKey);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);


protected:
    DocumentInfo uploadDocumentToDeepL(const std::string& filePath, const std::string& deepLKey);
    std::string checkDocumentStatus(const std::string& document_id, const std::string& document_key, const std::string& deepLKey);
    int handleDeepLRequest(const std::string& inputPath, const std::string& outputPath, const std::string& deepLKey);
    bool unzip_file(const std::string& zipPath, const std::string& outputDir);
    bool make_directory(const std::filesystem::path& path);
    std::string getNodePath(xmlNode *node);
    void extractTextNodesRecursive(xmlNode *node, std::vector<TextNode> &nodes);
    std::vector<TextNode> extractTextNodes(xmlNode *root);
    void saveTextToFile(const std::vector<TextNode> &nodes, const std::string &positionFilename, const std::string &textFilename);
    std::unordered_multimap<std::string, std::string> loadTranslations(const std::string &positionFilename, const std::string &textFilename);
    void updateNodeWithTranslation(xmlNode *node, const std::string &translation);
    void traverseAndReinsert(xmlNode *node, std::unordered_multimap<std::string, std::string> &translations, std::unordered_map<std::string, std::unordered_multimap<std::string, std::string>::iterator> &lastUsed);
    void reinsertTranslations(xmlNode *root, std::unordered_multimap<std::string, std::string> &translations);
    void exportDocx(const std::string& exportPath, const std::string& outputDir);
    std::string escapeForDocx(const std::string& input);
    void escapeTranslations(std::unordered_multimap<std::string, std::string>& translations);
    bool downloadTranslatedDocument(const std::string& document_id, const std::string& document_key, const std::string& deepLKey, const std::string& outputPath);
};