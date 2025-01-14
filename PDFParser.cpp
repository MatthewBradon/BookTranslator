#include "PDFParser.h"

void PDFParser::run() {
    std::cout << "Hello from PDFParser!\n";

    std::string pdfFilePath = "/Users/xsmoked/Downloads/Konosuba Volume 1 [JP].pdf";

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


// Splits long sentences into smaller ones at "breakpoints" if the chunk exceeds maxLength
std::vector<std::string> PDFParser::splitLongSentences(const std::string &sentence, size_t maxLength) {
    // Potential "logical" breakpoints: 、「", "。", "しかし", etc.
    // For simplicity, let’s treat these punctuation marks as possible breakpoints:
    // (In practice, you could also look for specific words or patterns.)
    const std::vector<std::string> breakpoints = {"、", "。", "しかし", "そして", "だから", "そのため"};

    std::vector<std::string> results;
    std::string currentChunk;
    currentChunk.reserve(sentence.size());

    size_t chunkLength = 0;

    // We iterate by characters; in C++, we have to be careful with UTF-8.
    // For simplicity, assume this is already a well-formed UTF-8 string,
    // and we’ll do a naive approach here. In real code, consider using an ICU-based approach
    // or a library that can handle wide chars properly.
    //
    // If you do morphological tokenization, you'll get tokens already split at morpheme boundaries.
    // Then you can iterate token-by-token, instead of character-by-character.

    // We'll do a super-simplified approach here:
    for (size_t i = 0; i < sentence.size(); ++i) {
        // Append current character to chunk
        currentChunk.push_back(sentence[i]);
        chunkLength++;

        // Check if this character is any of our breakpoints
        // (We do a naive check to see if the substring at position i
        // matches any of the multi-character breakpoints too.)
        bool isBreakpoint = false;
        for (auto &bp : breakpoints) {
            // If there's enough space left in sentence to compare
            if (i + bp.size() <= sentence.size()) {
                if (sentence.compare(i, bp.size(), bp) == 0) {
                    isBreakpoint = true;
                    break;
                }
            }
        }

        // If we're on a breakpoint AND the chunk is longer than maxLength, split here
        if (isBreakpoint && chunkLength > maxLength) {
            results.push_back(std::regex_replace(currentChunk, std::regex("^\\s+|\\s+$"), "")); // trim
            currentChunk.clear();
            chunkLength = 0;
        }
        // Otherwise, if we exceed maxLength (even without a breakpoint), force split
        else if (chunkLength > maxLength) {
            results.push_back(std::regex_replace(currentChunk, std::regex("^\\s+|\\s+$"), "")); // trim
            currentChunk.clear();
            chunkLength = 0;
        }
    }

    // Add any leftover text
    if (!currentChunk.empty()) {
        results.push_back(std::regex_replace(currentChunk, std::regex("^\\s+|\\s+$"), ""));
    }

    return results;
}

// Morphologically tokenize Japanese text using MeCab
// and then apply your splitting logic on punctuation or quotes.
std::vector<std::string> PDFParser::splitJapaneseText(const std::string &text) {
    // Create MeCab tagger
    MeCab::Tagger *tagger = MeCab::createTagger("");
    if (!tagger) {
        std::cerr << "Failed to create MeCab tagger." << std::endl;
        return {};
    }

    // We’ll hold final "sentences" here
    std::vector<std::string> sentences;

    // For quote detection
    bool inQuote = false;
    std::string currentSentence;

    // Parse the text
    const MeCab::Node* node = tagger->parseToNode(text.c_str());
    if (!node) {
        std::cerr << "Failed to parse text with MeCab." << std::endl;
        return {};
    }

    // Iterate over MeCab tokens
    for (; node; node = node->next) {
        if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) {
            // Skip beginning-of-sentence/end-of-sentence tokens
            continue;
        }

        // surface is the actual string for this token
        std::string tokenStr(node->surface, node->length);

        currentSentence += tokenStr;

        if (tokenStr == "「") {
            inQuote = true;
        } else if (tokenStr == "」") {
            inQuote = false;
        }

        // Check for sentence delimiters if not in quotes
        if (!inQuote && (tokenStr == "。" || tokenStr == "！" || tokenStr == "？")) {
            // If the current sentence is too long, split into smaller ones
            if (currentSentence.size() > 300) {
                auto splitted = splitLongSentences(currentSentence, 300);
                sentences.insert(sentences.end(), splitted.begin(), splitted.end());
            } else {
                // Just add the sentence
                sentences.push_back(currentSentence);
            }
            currentSentence.clear();
        }
    }

    // If there's leftover text, add it
    if (!currentSentence.empty()) {
        if (currentSentence.size() > 300) {
            auto splitted = splitLongSentences(currentSentence, 300);
            sentences.insert(sentences.end(), splitted.begin(), splitted.end());
        } else {
            sentences.push_back(currentSentence);
        }
    }

    return sentences;
}
