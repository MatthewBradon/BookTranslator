#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "PDFTranslator.h"

int PDFTranslator::run(const std::string& inputPath, const std::string& outputPath, int localModel, const std::string& deepLKey, std::string langcode) {
    // Check if the main temp files exist and delete them
    const std::string rawTextFilePath = "pdftext.txt";
    const std::string extractedTextPath = "extractedPDFtext.txt";
    const std::string imagesDir = "FilteredImages";
    std::string outputPdfPath = outputPath + "/output.pdf";
    std::string translatedTagsPath = "translatedTags.txt";

    std::filesystem::path rawTextFilePathPath = std::filesystem::u8path(rawTextFilePath);
    std::filesystem::path extractedTextPathPath = std::filesystem::u8path(extractedTextPath);
    std::filesystem::path imagesDirPath = std::filesystem::u8path(imagesDir);
    std::filesystem::path translatedTagsPathPath = std::filesystem::u8path(translatedTagsPath);


    // Check if the temp files exist and delete them
    if (std::filesystem::exists(rawTextFilePathPath)) {
        std::filesystem::remove(rawTextFilePathPath);
    }
    if (std::filesystem::exists(extractedTextPathPath)) {
        std::filesystem::remove(extractedTextPathPath);
    }
    if (std::filesystem::exists(imagesDirPath)) {
        std::filesystem::remove_all(imagesDirPath);
    }
    if (std::filesystem::exists(translatedTagsPathPath)) {
        std::filesystem::remove(translatedTagsPathPath);
    }


    // Ensure the images directory exists
    if (!std::filesystem::exists(imagesDir)) {
        std::filesystem::create_directory(imagesDir);
    }

    // Delete all the images in the imagesDir
    if (std::filesystem::exists(imagesDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(imagesDir)) {
            std::filesystem::remove(entry.path());
        }
    }

    std::filesystem::path currentDirPath = std::filesystem::current_path();


    // Convert the PDF to images
    convertPdfToImages(inputPath, imagesDir, 50.0f);

    std::cout << "Finished converting PDF to images" << '\n';

    // Extracts text from the PDF and writes it to extractedPDFtext.txt
    extractTextFromPDF(inputPath, extractedTextPath);

    



    try {
        // Splits the merged japanese into sentence and writes it to pdftext.txt
        std::vector<std::string> sentences = processAndSplitText(extractedTextPath, 300);

        // Open the output file
        std::ofstream outputFile(rawTextFilePath);
        if (!outputFile.is_open()) {
            throw std::runtime_error("Failed to open output file: " + rawTextFilePath);
        }

        // Handle DeepL request
        if (localModel == 1) {
            if (deepLKey.empty()) {
                std::cerr << "No DeepL API key provided." << std::endl;
                return 1;
            }

            // Write each sentence to the file with a corresponding number
            for (size_t i = 0; i < sentences.size(); ++i) {
                outputFile << sentences[i] << "\n";
            }

            // // Test writing to the file limit the number of sentences
            // for (size_t i = 0; i < 5; ++i) {
            //     outputFile << sentences[i] << "\n";
            // }
            

            outputFile.close();

            const std::string translatedTextPath = "translatedDeepL.txt";


            int result = handleDeepLRequest(rawTextFilePath, translatedTextPath, deepLKey);

            if (result != 0) {
                std::cerr << "Failed to handle DeepL request." << std::endl;
                return 1;
            }

            // Check if the translated text file exists
            if (!std::filesystem::exists(translatedTextPath)) {
                std::cerr << "Translated text file not found: " << translatedTextPath << std::endl;
                return 1;
            }

            // Open the translated text file and read in each line as a sentence but skip lines that are just \n
            std::ifstream translatedFile(translatedTextPath);

            if (!translatedFile.is_open()) {
                std::cerr << "Failed to open translated text file: " << translatedTextPath << std::endl;
                return 1;
            }

            std::vector<std::string> translatedSentences;
            std::string line;

            while (std::getline(translatedFile, line)) {
                if (!line.empty() && line.find_first_not_of(" \t\r\n") != std::string::npos) {
                    translatedSentences.push_back(line);
                }
            }

            translatedFile.close();

            // Write the translated sentence back to translatedTextPath
            std::ofstream translatedOutputFile(translatedTextPath);

            if (!translatedOutputFile.is_open()) {
                std::cerr << "Failed to open translated text file for writing: " << translatedTextPath << std::endl;
                return 1;
            }

            for (const auto& sentence : translatedSentences) {
                translatedOutputFile << sentence << "\n";
            }

            translatedOutputFile.close();

            
            createPDF(outputPdfPath, translatedTextPath, imagesDir);
            
            if (std::filesystem::exists(rawTextFilePathPath)) {
                std::filesystem::remove(rawTextFilePathPath);
            }
            if (std::filesystem::exists(extractedTextPathPath)) {
                std::filesystem::remove(extractedTextPathPath);
            }
            if (std::filesystem::exists(imagesDirPath)) {
                std::filesystem::remove_all(imagesDirPath);
            }
            if (std::filesystem::exists(translatedTagsPathPath)) {
                std::filesystem::remove(translatedTagsPathPath);
            }

            return 0;

        }


        // Write each sentence to the file with a corresponding number
        for (size_t i = 0; i < sentences.size(); ++i) {
            outputFile << (i + 1) << ",>>" << langcode << "<< " << sentences[i] << "\n";
        }

        outputFile.close();

    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "Finished splitting text" << '\n';
    std::string chapterNumberMode = "1";    // PDF does not have chapters, so use mode 1
    
    //Start the multiprocessing translaton
    std::filesystem::path translationExe;

    #if defined(__APPLE__)
        translationExe = currentDirPath / "translation";

    #elif defined(_WIN32)
        translationExe = currentDirPath / "translation.exe";
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
                rawTextFilePath,
                chapterNumberMode,
                boost::process::std_out > pipe_stdout, 
                boost::process::std_err > pipe_stderr,
                boost::process::windows::hide
            );
        #else
            boost::process::child c(
                translationExePath,
                rawTextFilePath,
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

    

    try {
        createPDF(outputPdfPath, "translatedTags.txt", imagesDir);
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }

    if (std::filesystem::exists(rawTextFilePathPath)) {
        std::filesystem::remove(rawTextFilePathPath);
    }
    if (std::filesystem::exists(extractedTextPathPath)) {
        std::filesystem::remove(extractedTextPathPath);
    }
    if (std::filesystem::exists(imagesDirPath)) {
        std::filesystem::remove_all(imagesDirPath);
    }
    if (std::filesystem::exists(translatedTagsPathPath)) {
        std::filesystem::remove(translatedTagsPathPath);
    }

    std::cout << "Finished" << std::endl;

    return 0;
}

std::string PDFTranslator::removeWhitespace(const std::string &input) {
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

fz_context* PDFTranslator::createMuPDFContext() {
    fz_context* ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    if (!ctx) {
        throw std::runtime_error("Failed to create MuPDF context.");
    }
    fz_register_document_handlers(ctx);
    return ctx;
}


void PDFTranslator::extractTextFromPage(fz_context* ctx, fz_document* doc, int pageIndex, std::ofstream& outputFile) {
    fz_page* page = nullptr;
    fz_stext_page* textPage = nullptr;
    fz_device* textDevice = nullptr;

    fz_try(ctx) {
        page = fz_load_page(ctx, doc, pageIndex);
        if (!page) {
            std::cerr << "Failed to load page " << pageIndex + 1 << "." << std::endl;
            return;
        }

        fz_rect bounds = fz_bound_page(ctx, page);
        textPage = fz_new_stext_page(ctx, bounds);
        fz_stext_options options = { 0 };
        textDevice = fz_new_stext_device(ctx, textPage, &options);

        fz_run_page(ctx, page, textDevice, fz_identity, NULL);

        if (!pageContainsText(textPage)) {
            return;
        }

        extractTextFromBlocks(textPage, outputFile);

    } fz_always(ctx) {
        if (textDevice) fz_drop_device(ctx, textDevice);
        if (textPage) fz_drop_stext_page(ctx, textPage);
        if (page) fz_drop_page(ctx, page);
    } fz_catch(ctx) {
        std::cerr << "Error extracting text from page " << pageIndex + 1 << std::endl;
    }
}

bool PDFTranslator::pageContainsText(fz_stext_page* textPage) {
    for (fz_stext_block* block = textPage->first_block; block; block = block->next) {
        if (block->type == FZ_STEXT_BLOCK_TEXT) {
            return true;
        }
    }
    return false;
}


void PDFTranslator::extractTextFromBlocks(fz_stext_page* textPage, std::ofstream& outputFile) {
    for (fz_stext_block* block = textPage->first_block; block; block = block->next) {
        if (block->type == FZ_STEXT_BLOCK_TEXT) {
            extractTextFromLines(block, outputFile);
        }
    }
}

void PDFTranslator::extractTextFromLines(fz_stext_block* block, std::ofstream& outputFile) {
    for (fz_stext_line* line = block->u.t.first_line; line; line = line->next) {
        extractTextFromChars(line, outputFile);
    }
}


void PDFTranslator::extractTextFromChars(fz_stext_line* line, std::ofstream& outputFile) {
    for (fz_stext_char* ch = line->first_char; ch; ch = ch->next) {
        char utf8[5] = { 0 };
        int len = fz_runetochar(utf8, ch->c);
        outputFile.write(utf8, len);
    }
}


void PDFTranslator::processPDF(fz_context* ctx, const std::string& inputPath, std::ofstream& outputFile) {
    fz_document* doc = nullptr;

    // Check if the PDF file exists
    std::filesystem::path inputPDFPath = std::filesystem::u8path(inputPath);

    if (!std::filesystem::exists(inputPDFPath)) {
        throw std::runtime_error("PDF file does not exist: " + inputPDFPath.string());
    }

    fz_try(ctx) {
        doc = fz_open_document(ctx, inputPath.c_str());
        if (!doc) {
            throw std::runtime_error("Failed to open PDF file: " + inputPath);
        }

        int pageCount = fz_count_pages(ctx, doc);
        std::cout << "Total pages: " << pageCount << std::endl;

        for (int i = 0; i < pageCount; ++i) {
            extractTextFromPage(ctx, doc, i, outputFile);
        }

        fz_drop_document(ctx, doc);
    } fz_catch(ctx) {
        if (doc) fz_drop_document(ctx, doc);
        throw std::runtime_error("An error occurred while processing the PDF.");
    }
}

void PDFTranslator::extractTextFromPDF(const std::string& inputPath, const std::string& outputFilePath) {
    std::cout << "Extracting text from PDF..." << std::endl;
    std::cout << "PDF file: " << inputPath << std::endl;
    std::cout << "Output file: " << outputFilePath << std::endl;

    auto startTime = std::chrono::high_resolution_clock::now();

    std::filesystem::path pdfPath = std::filesystem::u8path(inputPath);

    if (!std::filesystem::exists(pdfPath)) {
        throw std::runtime_error("PDF file does not exist: " + pdfPath.string());
    }

    fz_context* ctx = createMuPDFContext();
    std::ofstream outputFile(outputFilePath, std::ios::out | std::ios::binary);
    if (!outputFile) {
        throw std::runtime_error("Failed to open output file: " + outputFilePath);
    }

    processPDF(ctx, inputPath, outputFile);

    fz_drop_context(ctx);

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = endTime - startTime;
    std::cout << "Text extracted from PDF in " << duration.count() << " seconds." << std::endl;
}

// Helper function to determine the number of bytes in a UTF-8 character
size_t PDFTranslator::getUtf8CharLength(unsigned char firstByte) {
    // First byte of a UTF-8 character determines the number of bytes
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

std::vector<std::string> PDFTranslator::splitLongSentences(const std::string& sentence, size_t maxLength) {
    // You can adjust which words should "start" a new chunk vs. which punctuation 
    // should "end" the current chunk.
    // endBreakpoints appended to current chunk, then split.
    // startBreakpoints cause us to split BEFORE them, then start a new chunk WITH them.
    std::vector<std::string> endBreakpoints   = { "、", "。"};
    std::vector<std::string> startBreakpoints = { "しかし", "そして", "だから", "そのため" };

    std::vector<std::string> results;
    std::string currentChunk;
    size_t i = 0;

    while (i < sentence.size()) {
        // Determine the length of the next UTF-8 character safely
        size_t charLen = getUtf8CharLength(static_cast<unsigned char>(sentence[i]));

        // Safety check if the UTF-8 seems truncated near the end of the string
        if (i + charLen > sentence.size()) {
            // Add whatever is left as-is
            currentChunk += sentence.substr(i);
            break;
        }

        // We'll look ahead at the substring starting at i to see if it matches
        // any start breakpoint or end breakpoint.
        std::string_view nextSubstring(&sentence[i], sentence.size() - i);

        bool foundStartBreakpoint = false;
        bool foundEndBreakpoint   = false;
        std::string matchedBreakpoint;

        // Check "startBreakpoints" first
        for (auto&& sb : startBreakpoints) {
            if (nextSubstring.compare(0, sb.size(), sb) == 0) {
                foundStartBreakpoint = true;
                matchedBreakpoint    = sb;
                break;
            }
        }

        // Check endBreakpoints only if we did not match a start-breakpoint
        if (!foundStartBreakpoint) {
            for (auto&& eb : endBreakpoints) {
                if (nextSubstring.compare(0, eb.size(), eb) == 0) {
                    foundEndBreakpoint = true;
                    matchedBreakpoint  = eb;
                    break;
                }
            }
        }

        if (foundStartBreakpoint) {
            // Found a starting breakpoint
            // End the current chunk before this word (if currentChunk not empty)
            // Start a new chunk with that word included.

            // If there's already something accumulated, push it out
            if (!currentChunk.empty()) {
                results.push_back(currentChunk);
                currentChunk.clear();
            }

            // Now start a new chunk that *begins* with the matched breakpoint
            currentChunk += matchedBreakpoint;
            i += matchedBreakpoint.size();

        } else if (foundEndBreakpoint) {
            // We have encountered punctuation like "、" or "。"
            //  -> Add that punctuation to the current chunk
            //  -> End the chunk immediately after.

            currentChunk += matchedBreakpoint;
            i += matchedBreakpoint.size();

            // Now push this chunk
            results.push_back(currentChunk);
            currentChunk.clear();

        } else {
            // Just a normal character append to currentChunk
            std::string nextChar = sentence.substr(i, charLen);
            currentChunk += nextChar;
            i += charLen;

            // Check if currentChunk reached maxLength
            if (currentChunk.size() >= maxLength) {
                results.push_back(currentChunk);
                currentChunk.clear();
            }
        }
    }

    // If something remains in currentChunk, push it out
    if (!currentChunk.empty()) {
        results.push_back(currentChunk);
    }

    return results;
}

// Function to intelligently split Japanese text into sentences
std::vector<std::string> PDFTranslator::splitJapaneseText(const std::string& text, size_t maxLength) {
    std::vector<std::string> sentences;
    std::string currentSentence;
    bool inQuote = false;

    for (size_t i = 0; i < text.size();) {
        size_t charLength = getUtf8CharLength(static_cast<unsigned char>(text[i]));
        if (i + charLength > text.size()) break; // Safety check

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

std::vector<std::string> PDFTranslator::processAndSplitText(const std::string& inputFilePath, size_t maxLength) {
    
    std::filesystem::path inputPath = std::filesystem::u8path(inputFilePath);
    // Check if the input file exists
    if (!std::filesystem::exists(inputPath)) {
        throw std::runtime_error("Input file does not exist: " + inputPath.string());
    }

    // Open the input file
    std::ifstream inputFile(inputFilePath);
    if (!inputFile.is_open()) {
        throw std::runtime_error("Failed to open input file: " + inputFilePath);
    }

    // Read the entire file content into a string
    std::string text((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
    inputFile.close();

    // Process and split the text into sentences
    return splitJapaneseText(text, maxLength);
}

void PDFTranslator::convertPdfToImages(const std::string &pdfPath, const std::string &outputFolder, float stdDevThreshold) {
    // Check if the PDF file exists
    std::filesystem::path pdfFilePath = std::filesystem::u8path(pdfPath);

    if (!std::filesystem::exists(pdfFilePath)) {
        std::cerr << "PDF file does not exist: " << pdfFilePath.string() << std::endl;
        return;
    }


    fz_context* ctx = createMuPDFContext();

    fz_document* doc = fz_open_document(ctx, pdfPath.c_str());
    if (!doc) {
        std::cerr << "Failed to open PDF document: " << pdfPath << std::endl;
        fz_drop_context(ctx);
        return;
    }

    std::filesystem::create_directory(outputFolder);
    int pageCount = fz_count_pages(ctx, doc);

    for (int pageNum = 0; pageNum < pageCount; pageNum++) {
        fz_page* page = fz_load_page(ctx, doc, pageNum);
        fz_rect bounds = fz_bound_page(ctx, page);

        fz_pixmap* pixmap = fz_new_pixmap_from_page_number(ctx, doc, pageNum, fz_identity, fz_device_rgb(ctx), 1);

        if (!pixmap) {
            std::cerr << "Failed to create pixmap for page " << pageNum << std::endl;
            fz_drop_page(ctx, page);
            continue;
        }
        

        // Save the image temporarily
        char tempPath[1024];
        snprintf(tempPath, sizeof(tempPath), "%s/temp_page_%03d.png", outputFolder.c_str(), pageNum + 1);
        fz_save_pixmap_as_png(ctx, pixmap, tempPath);

        // Check if the image passes the threshold
        if (isImageAboveThreshold(tempPath, stdDevThreshold)) {
            char outputPath[1024];
            snprintf(outputPath, sizeof(outputPath), "%s/page_%03d.png", outputFolder.c_str(), pageNum + 1);
            std::filesystem::rename(tempPath, outputPath); // Save with final name
            std::cout << "Saved: " << outputPath << std::endl;
        } else {
            std::filesystem::remove(tempPath); // Delete temporary image
        }

        fz_drop_pixmap(ctx, pixmap);
        fz_drop_page(ctx, page);
    }

    fz_drop_document(ctx, doc);
    fz_drop_context(ctx);
}

bool PDFTranslator::isImageAboveThreshold(const std::string &imagePath, float threshold) {
    int width, height, channels;
    unsigned char* data = stbi_load(imagePath.c_str(), &width, &height, &channels, 1); // Load as grayscale
    if (!data) {
        std::cerr << "Failed to load image: " << imagePath << std::endl;
        return false;
    }

    // Calculate mean and standard deviation
    double sum = 0.0, sqSum = 0.0;
    int numPixels = width * height;

    for (int i = 0; i < numPixels; ++i) {
        double pixel = data[i];
        sum += pixel;
        sqSum += pixel * pixel;
    }

    double mean = sum / numPixels;
    double variance = (sqSum / numPixels) - (mean * mean);
    double stddev = std::sqrt(variance);

    std::cout << "Image: " << imagePath << " - Standard Deviation: " << stddev << std::endl;

    stbi_image_free(data); // Free the image memory
    return stddev > threshold;
}


std::pair<cairo_surface_t*, cairo_t*> PDFTranslator::initCairoPdfSurface(const std::string &filename, double width, double height) {
    cairo_surface_t* surface = cairo_pdf_surface_create(filename.c_str(), width, height);
    cairo_t* cr = cairo_create(surface);

    return {surface, cr};
}


void PDFTranslator::cleanupCairo(cairo_t* cr, cairo_surface_t* surface) {
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

std::vector<std::string> PDFTranslator::collectImageFiles(const std::string &images_dir) {
    std::vector<std::string> image_files;
    for (const auto &entry : std::filesystem::directory_iterator(images_dir)) {
        if (entry.is_regular_file()) {
            std::string path_str = entry.path().string();
            std::string ext = std::filesystem::path(path_str).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // to lowercase

            if (isImageFile(ext)) {
                image_files.push_back(path_str);
            } else {
                std::cout << "Skipping non-image file: " << path_str << std::endl;
            }
        }
    }

    std::sort(image_files.begin(), image_files.end());
    return image_files;
}

bool PDFTranslator::isImageFile(const std::string &extension) {
    return (extension == ".png" || extension == ".jpg" || 
            extension == ".jpeg"|| extension == ".bmp" || extension == ".tiff");
}


bool PDFTranslator::addImagesToPdf(cairo_t *cr, cairo_surface_t *surface, const std::vector<std::string> &image_files) {
    bool has_images = false;
    
    for (const auto &image_file : image_files) {
        // Try loading the image as PNG (change logic if you want to handle JPG, etc. differently)
        cairo_surface_t *image = cairo_image_surface_create_from_png(image_file.c_str());
        if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
            std::cerr << "Failed to load image: " << image_file << std::endl;
            cairo_surface_destroy(image);
            continue;
        }

        // Get image dimensions
        int image_width = cairo_image_surface_get_width(image);
        int image_height = cairo_image_surface_get_height(image);

        // Set the page size to match the image size
        cairo_pdf_surface_set_size(surface, image_width, image_height);

        // Draw the image at (0, 0)
        cairo_save(cr);
        cairo_set_source_surface(cr, image, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);

        // Start a new page for the next image
        cairo_show_page(cr);
        has_images = true;

        cairo_surface_destroy(image);
    }

    return has_images;
}

void PDFTranslator::configureTextRendering(cairo_t *cr, const std::string &font_family, double font_size) {
    cairo_select_font_face(cr, font_family.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, font_size);
}

void PDFTranslator::addTextToPdf(cairo_t* cr, cairo_surface_t* surface, const std::string &input_file, double page_width, double page_height, double margin, double line_spacing, double font_size) {
    std::ifstream infile(input_file);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open file " << input_file << std::endl;
        return;
    }

    double x = margin;
    double y = margin;
    double usable_width = page_width - 2 * margin;

    std::string line;
    while (std::getline(infile, line)) {
        size_t comma_pos = line.find(',');
        if (comma_pos == std::string::npos) {
            // If no comma found skip
            continue;
        }

        // Extract text after the comma
        std::string text = line.substr(comma_pos + 1);

        // Word wrapping
        std::istringstream text_stream(text);
        std::string word;
        std::string current_line;
        cairo_text_extents_t extents;

        while (text_stream >> word) {
            // Test if adding this word exceeds usable width
            std::string test_line = current_line.empty() ? word : (current_line + " " + word);
            cairo_text_extents(cr, test_line.c_str(), &extents);

            if (extents.width > usable_width) {
                // Render the current line
                cairo_move_to(cr, x, y);
                cairo_show_text(cr, current_line.c_str());
                y += font_size * line_spacing;

                // New page if we exceed the page limit
                if (y > page_height - margin) {
                    cairo_show_page(cr);
                    y = margin;
                }

                // Start a new line with the current word
                current_line = word;
            } else {
                // Keep adding to the current line
                current_line = test_line;
            }
        }

        // Render remaining text in the current line
        if (!current_line.empty()) {
            cairo_move_to(cr, x, y);
            cairo_show_text(cr, current_line.c_str());
            y += font_size * line_spacing;

            // Check for page overflow
            if (y > page_height - margin) {
                cairo_show_page(cr);
                y = margin;
            }
        }
    }
}

void PDFTranslator::createPDF(const std::string &output_file, const std::string &input_file, const std::string &images_dir) {
    // Check if the input file exists
    std::filesystem::path inputPath = std::filesystem::u8path(input_file);

    if (!std::filesystem::exists(inputPath)) {
        std::cerr << "Input file does not exist: " << inputPath.string() << std::endl;
        return;
    }
    
    
    const double page_width = 612.0;
    const double page_height = 792.0;
    const double margin = 72.0;
    const double line_spacing = 2.0;
    const double font_size = 12.0;
    const std::string font_family = "Helvetica";

    // Initialize Cairo PDF surface
    auto [surface, cr] = initCairoPdfSurface(output_file, page_width, page_height);

    // Collect and add images
    std::vector<std::string> image_files = collectImageFiles(images_dir);
    bool has_images = addImagesToPdf(cr, surface, image_files);

    // If images were added, reset surface to standard A4 for text
    if (has_images) {
        cairo_pdf_surface_set_size(surface, page_width, page_height);
    }

    // Configure text rendering and add text
    configureTextRendering(cr, font_family, font_size);
    addTextToPdf(cr, surface, input_file, page_width, page_height, margin, line_spacing, font_size);

    // Cleanup
    cleanupCairo(cr, surface);

    std::cout << "PDF created with images first: " << output_file << std::endl;
}

size_t PDFTranslator::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

std::string PDFTranslator::uploadDocumentToDeepL(const std::string& filePath, const std::string& deepLKey) {
    
    // Check if the file exists
    std::filesystem::path inputPath = std::filesystem::u8path(filePath);

    if (!std::filesystem::exists(inputPath)) {
        std::cerr << "File does not exist: " << inputPath.string() << std::endl;
        return "";
    }
    
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

    // Extract document_id and document_key from the response
    try {
        std::cout << "Document upload response: " << response_string << std::endl;
        nlohmann::json jsonResponse = nlohmann::json::parse(response_string);
        std::string document_id = jsonResponse["document_id"];
        std::string document_key = jsonResponse["document_key"];
        return document_id + "|" + document_key;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Error parsing document upload response: " << e.what() << std::endl;
        return "";
    }
}

std::string PDFTranslator::checkDocumentStatus(const std::string& document_id, const std::string& document_key, const std::string& deepLKey) {
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

std::string PDFTranslator::downloadTranslatedDocument(const std::string& document_id, const std::string& document_key, const std::string& deepLKey) {
    CURL* curl;
    CURLcode res;
    std::string response_string;
    std::string api_url = "https://api-free.deepl.com/v2/document/" + document_id + "/result";
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
            std::cerr << "Curl download request failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();

    return response_string;
}

int PDFTranslator::handleDeepLRequest(const std::string& inputPath, const std::string& outputPath, const std::string& deepLKey) {
    // Check if the input file exists
    std::filesystem::path inputPDFPath = std::filesystem::u8path(inputPath);

    if (!std::filesystem::exists(inputPDFPath)) {
        std::cerr << "Input file does not exist: " << inputPDFPath.string() << std::endl;
        return 1;
    }
    
    
    // Upload the document to DeepL
    std::string document_info = uploadDocumentToDeepL(inputPath, deepLKey);
    if (document_info.empty()) {
        std::cerr << "Failed to upload document to DeepL." << std::endl;
        return 1;
    }

    // Extract the document_id and document_key
    size_t separator_pos = document_info.find('|');
    std::string document_id = document_info.substr(0, separator_pos);
    std::string document_key = document_info.substr(separator_pos + 1);


    std::string status;
    bool isTranslationComplete = false;

    while (!isTranslationComplete) {
        status = checkDocumentStatus(document_id, document_key, deepLKey);
        // Parse the JSON response to check if status == "done"
        nlohmann::json jsonResponse = nlohmann::json::parse(status);
        if (jsonResponse["status"] == "done") {
            isTranslationComplete = true;
        } else {
            std::cout << "Translation in progress..." << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(5));  // Wait for 5 seconds before checking again
        }
    }

    // Download the translated document
    std::string download_response = downloadTranslatedDocument(document_id, document_key, deepLKey);
    if (download_response.empty()) {
        std::cerr << "Failed to download translated document." << std::endl;
        return 1;
    }

    // Save the translated document to the output path
    std::ofstream output_file(outputPath);
    if (!output_file.is_open()) {
        std::cerr << "Failed to open output file: " << outputPath << std::endl;
        return 1;
    }

    output_file << download_response;
    output_file.close();

    return 0;
}