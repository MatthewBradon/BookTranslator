#pragma once

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
#include <unordered_set>
#include <unordered_map>
#include "Document.h"
#include <libxml/HTMLtree.h>

#ifdef _WIN32
#include <boost/process/windows.hpp>
#endif

class HTMLTranslator : public Translator {
public:
    int run(const std::string& inputPath, const std::string& outputPath, int localModel, const std::string& deepLKey, std::string langcode);
protected:
    bool make_directory(const std::filesystem::path& path);
    std::string getNodePath(xmlNode* node);
    void extractTextNodesRecursive(xmlNode* node, std::vector<TextNode>& nodes);
    std::vector<TextNode> extractTextNodes(xmlNode* root);
    void saveTextToFile(const std::vector<TextNode>& nodes, const std::string& positionFilename, const std::string& textFilename, const std::string& langcode);
    std::unordered_multimap<std::string, std::string> loadTranslations(const std::string& positionFilename, const std::string& textFilename);
    void updateNodeWithTranslation(xmlNode* node, const std::string& translation);
    void traverseAndReinsert(xmlNode* node, std::unordered_multimap<std::string, std::string>& translations, std::unordered_map<std::string, std::unordered_multimap<std::string, std::string>::iterator>& lastUsed);
    void reinsertTranslations(xmlNode* root, std::unordered_multimap<std::string, std::string>& translations);
    std::string escapeForHtml(const std::string& input);
    void escapeTranslations(std::unordered_multimap<std::string, std::string>& translations);
};
