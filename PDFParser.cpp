#include "PDFParser.h"

void PDFParser::run() {
    std::cout << "Hello from PDFParser!\n";

    std::string pdfFilePath = "C:/Users/matth/Desktop/Kono Subarashii Sekai ni Shukufuku wo! [JP]/Konosuba Volume 2 [JP].pdf";
    // std::string pdfFilePath = "/Users/xsmoked/Downloads/Konosuba Volume 1 [JP].pdf";

    std::string fullText = extractTextFromPDF(pdfFilePath);

    if (fullText.empty()) {
        std::cerr << "Failed to extract text from PDF." << std::endl;
        return;
    }

    fullText = removeWhitespace(fullText);

    std::vector<std::string> sentences = splitJapaneseText(fullText);

    // Write the sentences to a file
    std::ofstream outputFile("pdftext.txt");

    if (!outputFile.is_open()) {
        std::cerr << "Failed to open output file for writing." << std::endl;
        return;
    }

    for (const auto &sentence : sentences) {
        outputFile << sentence << "\n";
    }

    outputFile.close();

    std::cout << "Text extracted from PDF and written to pdftext.txt\n";
    
    std::cout << "Finished" << std::endl;

    return;
}

std::string PDFParser::removeWhitespace(const std::string &input) {
    std::string result;
    result.reserve(input.size());
    
    for (char c : input) {
        // "isspace" checks for spaces, tabs, newlines, etc.
        if (!std::isspace(static_cast<unsigned char>(c))) {
            result.push_back(c);
        }
    }
    
    return result;
}

bool PDFParser::isMainImage() {
    return false;
}

void PDFParser::getMainImages() {
    return;
}

std::string PDFParser::extractTextFromPDF( std::string pdfFilePath) {
    
    std::string fullText = "";

    // Open the PDF file
    std::unique_ptr<poppler::document> doc(poppler::document::load_from_file(pdfFilePath));
    if (!doc) {
        std::cerr << "Error: Could not open PDF file '" << pdfFilePath << "'." << std::endl;
        return "";
    }

    // Get the number of pages in the PDF
    int numPages = doc->pages();
    std::cout << "Number of pages in PDF: " << numPages << std::endl;

    // Iterate over each page and extract text
    for (int i = 0; i < numPages; ++i) {
        std::unique_ptr<poppler::page> p(doc->create_page(i));
        if (!p) {
            std::cerr << "Error: Could not read page " << i << "." << std::endl;
            continue;
        }
        
        // Extract the Japanese text from the page
        poppler::byte_array rawText = p->text().to_utf8();
        std::string text(rawText.begin(), rawText.end());

        fullText += text;
        std::cout << text << std::endl;
    }


    std::cout << "Extracted text from PDF\n";
    
    return fullText;
}

void PDFParser::createPDF() {
    return;
}



// Function to split long sentences into smaller chunks based on logical breakpoints
std::vector<std::string> PDFParser::splitLongSentences(const std::string& sentence, size_t maxLength) {
    std::vector<std::string> breakpoints = {"、", "。", "しかし", "そして", "だから", "そのため"};
    std::vector<std::string> sentences;
    std::string currentChunk;
    size_t tokenCount = 0;

    for (size_t i = 0; i < sentence.size(); ++i) {
        currentChunk += sentence[i];
        ++tokenCount;

        // Check if the current character is a logical breakpoint
        for (const auto& breakpoint : breakpoints) {
            if (sentence.substr(i, breakpoint.size()) == breakpoint) {
                if (currentChunk.size() > maxLength) {
                    sentences.push_back(currentChunk);
                    currentChunk.clear();
                    tokenCount = 0;
                }
                break;
            }
        }

        // Force a split if the token count exceeds the maxLength
        if (tokenCount > maxLength) {
            sentences.push_back(currentChunk);
            currentChunk.clear();
            tokenCount = 0;
        }
    }

    // Add any remaining text as the last sentence
    if (!currentChunk.empty()) {
        sentences.push_back(currentChunk);
    }

    return sentences;
}

// Function to intelligently split Japanese text into sentences
std::vector<std::string> PDFParser::splitJapaneseText(const std::string& text, size_t maxLength) {
    std::vector<std::string> sentences;
    std::string currentSentence;
    bool inQuote = false;

    for (size_t i = 0; i < text.size(); ++i) {
        char currentChar = text[i];
        currentSentence += currentChar;

        // Handle opening and closing quotes
        if (std::string(1, currentChar) == "「") {
            inQuote = true;
        } else if (std::string(1, currentChar) == "」") {
            inQuote = false;
        }

        // Check for sentence-ending punctuation
        if ((std::string(1, currentChar) == "。" || std::string(1, currentChar) == "！" || std::string(1, currentChar) == "？") && !inQuote) {
            if (currentSentence.size() > maxLength) {
                // Handle long sentences
                auto splitChunks = splitLongSentences(currentSentence, maxLength);
                sentences.insert(sentences.end(), splitChunks.begin(), splitChunks.end());
            } else {
                sentences.push_back(currentSentence);
            }
            currentSentence.clear();
        }

        // Handle sentence-ending punctuation inside quotes
        if (i > 0 && std::string(1, text[i - 1]) == "」" && std::string(1, currentChar) == "。" && !inQuote) {
            if (currentSentence.size() > maxLength) {
                auto splitChunks = splitLongSentences(currentSentence, maxLength);
                sentences.insert(sentences.end(), splitChunks.begin(), splitChunks.end());
            } else {
                sentences.push_back(currentSentence);
            }
            currentSentence.clear();
        }
    }

    // Add any remaining text as the last sentence
    if (!currentSentence.empty()) {
        if (currentSentence.size() > maxLength) {
            auto splitChunks = splitLongSentences(currentSentence, maxLength);
            sentences.insert(sentences.end(), splitChunks.begin(), splitChunks.end());
        } else {
            sentences.push_back(currentSentence);
        }
    }

    return sentences;
}
