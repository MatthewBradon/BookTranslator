#pragma once

#include <vector>
#include <poppler-document.h>
#include <poppler-page.h>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <regex>
#include <iostream>
#include <string>
#include <memory>

class PDFParser {
public:
    void run();
private:
    std::string removeWhitespace(const std::string &input);
    bool isMainImage();
    void getMainImages();
    std::string extractTextFromPDF(std::string pdfFilePath);
    std::vector<std::string> splitLongSentences(const std::string &sentence, size_t maxLength = 300);
    std::vector<std::string> splitJapaneseText(const std::string& text, size_t maxLength = 300);
    void createPDF();

    char pdfToConvert[256] = "";
    char outputPath[256] = "";

};