#pragma once

#include <vector>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <regex>
#include <iostream>
#include <string>
#include <memory>
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <sstream>
#include <iostream>
#include <codecvt>
#include <locale>
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include <chrono>
#include <cairo.h>
#include <cairo-pdf.h>
#include "Translator.h"

class PDFTranslator : public Translator {
public:
    // Implement the run method from the Translator interface
    int run(const std::string& inputPath, const std::string& outputPath);

protected:
    // Private helper methods
    std::string removeWhitespace(const std::string& input);
    void extractTextFromPDF(const std::string& pdfFilePath, const std::string& outputFilePath);
    void processAndSplitText(const std::string& inputFilePath, const std::string& outputFilePath, size_t maxLength);
    std::vector<std::string> splitLongSentences(const std::string& sentence, size_t maxLength = 300);
    std::vector<std::string> splitJapaneseText(const std::string& text, size_t maxLength = 300);
    size_t getUtf8CharLength(unsigned char firstByte);
    void convertPdfToImages(const std::string& pdfPath, const std::string& outputFolder, float stdDevThreshold);
    bool isImageAboveThreshold(const std::string& imagePath, float threshold);
    void createPDF(const std::string& output_file, const std::string& text, const std::string& images_dir);
};