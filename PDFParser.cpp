#include "PDFParser.h"

int PDFParser::run() {

    std::string rawTextFilePath = "pdftext.txt";
    std::filesystem::path currentDirPath = std::filesystem::current_path();
    std::cout << "Hello from PDFParser!\n";

    std::string pdfFilePath = "C:/Users/matth/Desktop/Kono Subarashii Sekai ni Shukufuku wo! [JP]/Konosuba Volume 2 [JP].pdf";
    // std::string pdfFilePath = "/Users/xsmoked/Downloads/Konosuba Volume 1 [JP].pdf";

    std::string fullText = extractTextFromPDF(pdfFilePath);

    if (fullText.empty()) {
        std::cerr << "Failed to extract text from PDF." << std::endl;
        return 1;
    }

    fullText = removeWhitespace(fullText);

    // Write the extracted text to a file for debugging postRemoveWhitespace.txt
    std::ofstream postWhitespaceFile("postRemoveWhitespace.txt");

    if (!postWhitespaceFile.is_open()) {
        std::cerr << "Failed to open output file for writing." << std::endl;
        return 1;
    }

    postWhitespaceFile << fullText;

    postWhitespaceFile.close();


    std::vector<std::string> sentences = splitJapaneseText(fullText);

    // Write the sentences to a file
    std::ofstream outputFile(rawTextFilePath);

    if (!outputFile.is_open()) {
        std::cerr << "Failed to open output file for writing." << std::endl;
        return 1;
    }
    // Write the sentences to a txt file and assign a position id
    int position = 0;
    for (const auto &sentence : sentences) {
        // Skip sentence if it just contains whitespace
        if (sentence.find_first_not_of("。") == std::string::npos || sentence.find_first_not_of("？") == std::string::npos || sentence.find_first_not_of("！") == std::string::npos) {
            continue;
        }

        outputFile << std::to_string(position) << "," << sentence << "\n";
        position++;
    }

    outputFile.close();

    std::cout << "Text extracted from PDF and written to pdftext.txt\n";

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
    boost::filesystem::path inputFilePath = rawTextFilePath;  // Path to the input file
    std::string chapterNumberMode = "1";    // PDF does not have chapters, so use mode 1
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
        multiprocessExe = currentDirPath / "multiprocessTranslation";

    #elif defined(_WIN32)
        multiprocessExe = currentDirPath / "multiprocessTranslation.exe";
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
    
    std::cout << "Finished" << std::endl;

    return 0;
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

    // Write out the extracted text to a file for debugging extractedText.txt
    std::ofstream outputFile("extractedText.txt");

    if (!outputFile.is_open()) {
        std::cerr << "Failed to open output file for writing." << std::endl;
        return "";
    }

    outputFile << fullText;

    outputFile.close();


    std::cout << "Extracted text from PDF\n";
    
    return fullText;
}

void PDFParser::createPDF() {
    return;
}



// // Function to split long sentences into smaller chunks based on logical breakpoints
// std::vector<std::string> PDFParser::splitLongSentences(const std::string& sentence, size_t maxLength) {
//     std::vector<std::string> breakpoints = {"、", "。", "しかし", "そして", "だから", "そのため"};
//     std::vector<std::string> sentences;
//     std::string currentChunk;
//     size_t tokenCount = 0;

//     for (size_t i = 0; i < sentence.size(); ++i) {
//         currentChunk += sentence[i];
//         ++tokenCount;

//         // Check if the current character is a logical breakpoint
//         for (const auto& breakpoint : breakpoints) {
//             if (sentence.substr(i, breakpoint.size()) == breakpoint) {
//                 if (currentChunk.size() > maxLength) {
//                     sentences.push_back(currentChunk);
//                     currentChunk.clear();
//                     tokenCount = 0;
//                 }
//                 break;
//             }
//         }

//         // Force a split if the token count exceeds the maxLength
//         if (tokenCount > maxLength) {
//             sentences.push_back(currentChunk);
//             currentChunk.clear();
//             tokenCount = 0;
//         }
//     }

//     // Add any remaining text as the last sentence
//     if (!currentChunk.empty()) {
//         sentences.push_back(currentChunk);
//     }

//     return sentences;
// }

// // Function to intelligently split Japanese text into sentences
// std::vector<std::string> PDFParser::splitJapaneseText(const std::string& text, size_t maxLength) {
//     std::vector<std::string> sentences;
//     std::string currentSentence;
//     bool inQuote = false;

//     for (size_t i = 0; i < text.size(); ++i) {
//         char currentChar = text[i];
//         currentSentence += currentChar;

//         // Handle opening and closing quotes
//         if (std::string(1, currentChar) == "「") {
//             inQuote = true;
//         } else if (std::string(1, currentChar) == "」") {
//             inQuote = false;
//         }

//         // Check for sentence-ending punctuation
//         if ((std::string(1, currentChar) == "。" || std::string(1, currentChar) == "！" || std::string(1, currentChar) == "？") && !inQuote) {
//             if (currentSentence.size() > maxLength) {
//                 // Handle long sentences
//                 auto splitChunks = splitLongSentences(currentSentence, maxLength);
//                 sentences.insert(sentences.end(), splitChunks.begin(), splitChunks.end());
//             } else {
//                 sentences.push_back(currentSentence);
//             }
//             currentSentence.clear();
//         }

//         // Handle sentence-ending punctuation inside quotes
//         if (i > 0 && std::string(1, text[i - 1]) == "」" && std::string(1, currentChar) == "。" && !inQuote) {
//             if (currentSentence.size() > maxLength) {
//                 auto splitChunks = splitLongSentences(currentSentence, maxLength);
//                 sentences.insert(sentences.end(), splitChunks.begin(), splitChunks.end());
//             } else {
//                 sentences.push_back(currentSentence);
//             }
//             currentSentence.clear();
//         }
//     }

//     // Add any remaining text as the last sentence
//     if (!currentSentence.empty()) {
//         if (currentSentence.size() > maxLength) {
//             auto splitChunks = splitLongSentences(currentSentence, maxLength);
//             sentences.insert(sentences.end(), splitChunks.begin(), splitChunks.end());
//         } else {
//             sentences.push_back(currentSentence);
//         }
//     }

//     return sentences;
// }


// Helper function to determine the number of bytes in a UTF-8 character
size_t PDFParser::getUtf8CharLength(unsigned char firstByte) {
    if ((firstByte & 0b10000000) == 0) {
        return 1; // 1-byte character (0xxxxxxx)
    } else if ((firstByte & 0b11100000) == 0b11000000) {
        return 2; // 2-byte character (110xxxxx)
    } else if ((firstByte & 0b11110000) == 0b11100000) {
        return 3; // 3-byte character (1110xxxx)
    } else if ((firstByte & 0b11111000) == 0b11110000) {
        return 4; // 4-byte character (11110xxx)
    } else {
        throw std::runtime_error("Invalid UTF-8 encoding detected");
    }
}

// Function to split long sentences into smaller chunks based on logical breakpoints
std::vector<std::string> PDFParser::splitLongSentences(const std::string& sentence, size_t maxLength) {
    std::vector<std::string> breakpoints = {"、", "。", "しかし", "そして", "だから", "そのため"};
    std::vector<std::string> sentences;
    std::string currentChunk;
    size_t tokenCount = 0;

    for (size_t i = 0; i < sentence.size();) {
        size_t charLength = getUtf8CharLength(static_cast<unsigned char>(sentence[i]));
        currentChunk += sentence.substr(i, charLength);
        ++tokenCount;

        // Check if the current substring matches any breakpoint
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

        // Force a split if the token count exceeds maxLength
        if (tokenCount > maxLength) {
            sentences.push_back(currentChunk);
            currentChunk.clear();
            tokenCount = 0;
        }

        i += charLength;
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

    for (size_t i = 0; i < text.size();) {
        size_t charLength = getUtf8CharLength(static_cast<unsigned char>(text[i]));
        std::string currentChar = text.substr(i, charLength);
        currentSentence += currentChar;

        // Handle opening and closing quotes
        if (currentChar == "「") {
            inQuote = true;
        } else if (currentChar == "」") {
            inQuote = false;
        }

        // Check for sentence-ending punctuation
        if ((currentChar == "。" || currentChar == "！" || currentChar == "？") && !inQuote) {
            if (currentSentence.size() > maxLength) {
                auto splitChunks = splitLongSentences(currentSentence, maxLength);
                sentences.insert(sentences.end(), splitChunks.begin(), splitChunks.end());
            } else {
                sentences.push_back(currentSentence);
            }
            currentSentence.clear();
        }

        // Handle sentence-ending punctuation inside quotes
        if (i > 0 && text.substr(i - charLength, charLength) == "」" && currentChar == "。" && !inQuote) {
            if (currentSentence.size() > maxLength) {
                auto splitChunks = splitLongSentences(currentSentence, maxLength);
                sentences.insert(sentences.end(), splitChunks.begin(), splitChunks.end());
            } else {
                sentences.push_back(currentSentence);
            }
            currentSentence.clear();
        }

        i += charLength;
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