#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "PDFTranslator.h"

int PDFTranslator::run(const std::string& inputPath, const std::string& outputPath, int localModel, const std::string& deepLKey) {
    // Check if the main temp files exist and delete them
    const std::string rawTextFilePath = "pdftext.txt";
    const std::string extractedTextPath = "extractedPDFtext.txt";
    const std::string imagesDir = "FilteredImages";
    std::string outputPdfPath = outputPath + "/output.pdf";

    // Check if the temp files exist and delete them
    if (std::filesystem::exists(rawTextFilePath)) {
        std::filesystem::remove(rawTextFilePath);
    }

    if (std::filesystem::exists(extractedTextPath)) {
        std::filesystem::remove(extractedTextPath);
    }

    if (std::filesystem::exists("encodedTags.txt")) {
        std::filesystem::remove("encodedTags.txt");
    }

    if (std::filesystem::exists("translatedTags.txt")) {
        std::filesystem::remove("translatedTags.txt");
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
    std::cout << "Hello from PDFTranslator!\n";


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
            // for (size_t i = 0; i < sentences.size(); ++i) {
            //     outputFile << sentences[i] << "\n";
            // }

            // Test writing to the file limit the number of sentences
            for (size_t i = 0; i < 5; ++i) {
                outputFile << sentences[i] << "\n";
            }
            

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
            

            return 0;

        }


        // Write each sentence to the file with a corresponding number
        for (size_t i = 0; i < sentences.size(); ++i) {
            outputFile << (i + 1) << "," << sentences[i] << "\n";
        }

        outputFile.close();

    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "Finished splitting text" << '\n';

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

    boost::filesystem::path inputFilePath = rawTextFilePath;  // Path to the input file (pdftext.txt)
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

    

    try {
        createPDF(outputPdfPath, "translatedTags.txt", imagesDir);
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
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

void PDFTranslator::extractTextFromPDF(const std::string& inputPath, const std::string& outputFilePath) {

    std::cout << "Extracting text from PDF..." << std::endl;
    std::cout << "PDF file: " << inputPath << std::endl;
    std::cout << "Output file: " << outputFilePath << std::endl;

    auto startTime = std::chrono::high_resolution_clock::now();


    // Check if the file exists
    if (!std::filesystem::exists(inputPath)) {
        throw std::runtime_error("PDF file does not exist: " + inputPath);
    }

    // Create a MuPDF context
    fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
    if (!ctx) {
        throw std::runtime_error("Failed to create MuPDF context.");
    }

    // Register document handlers
    fz_register_document_handlers(ctx);

    std::ofstream outputFile(outputFilePath, std::ios::out | std::ios::binary);
    if (!outputFile) {
        throw std::runtime_error("Failed to open output file: " + outputFilePath);
    }

    fz_try(ctx) {
        // Open the PDF document
        fz_document *doc = fz_open_document(ctx, inputPath.c_str());
        if (!doc) {
            throw std::runtime_error("Failed to open PDF file: " + inputPath);
        }

        int pageCount = fz_count_pages(ctx, doc);
        std::cout << "Total pages: " << pageCount << std::endl;

        for (int i = 0; i < pageCount; ++i) {
            // Load the page
            fz_page *page = fz_load_page(ctx, doc, i);
            if (!page) {
                std::cerr << "Failed to load page " << i + 1 << "." << std::endl;
                continue;
            }

            // Get the page bounds
            fz_rect bounds = fz_bound_page(ctx, page);

            // Create a structured text page and device
            fz_stext_page *textPage = fz_new_stext_page(ctx, bounds);
            fz_stext_options options = { 0 };

            fz_device *textDevice = fz_new_stext_device(ctx, textPage, &options);

            fz_try(ctx) {
                // Run the page through the text extraction device
                fz_run_page(ctx, page, textDevice, fz_identity, NULL);

                // Check if the page contains any text
                bool hasText = false;
                for (fz_stext_block *block = textPage->first_block; block; block = block->next) {
                    if (block->type == FZ_STEXT_BLOCK_TEXT) {
                        hasText = true;
                        break;
                    }
                }

                // Skip the page if no text is found
                if (!hasText) {
                    continue;
                }

                // Extract text from the structured text page
                for (fz_stext_block *block = textPage->first_block; block; block = block->next) {
                    if (block->type == FZ_STEXT_BLOCK_TEXT) {
                        for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
                            for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
                                // Write the UTF-8 character to the file
                                char utf8[5] = { 0 }; // Buffer for UTF-8 encoding
                                int len = fz_runetochar(utf8, ch->c); // Convert to UTF-8
                                outputFile.write(utf8, len);
                            }
                            // outputFile.put('\n'); // Newline at the end of each line
                        }
                    }
                }
            } fz_always(ctx) {
                fz_drop_device(ctx, textDevice);
                fz_drop_stext_page(ctx, textPage);
                fz_drop_page(ctx, page);
            } fz_catch(ctx) {
                std::cerr << "Error extracting text from page " << i + 1 << std::endl;
            }
        }

        fz_drop_document(ctx, doc);
    } fz_catch(ctx) {
        fz_drop_context(ctx);
        throw std::runtime_error("An error occurred while processing the PDF.");
    }

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

// Function to split long sentences into smaller chunks based on logical breakpoints
std::vector<std::string> PDFTranslator::splitLongSentences(const std::string& sentence, size_t maxLength) {
    std::vector<std::string> breakpoints = {"、", "。", "しかし", "そして", "だから", "そのため"};
    std::vector<std::string> sentences;
    std::string currentChunk;
    size_t tokenCount = 0;

    for (size_t i = 0; i < sentence.size();) {
        size_t charLength = getUtf8CharLength(static_cast<unsigned char>(sentence[i]));
        if (i + charLength > sentence.size()) break; // Safety check

        currentChunk += sentence.substr(i, charLength);
        ++tokenCount;

        // Check if the current substring matches any breakpoint
        for (const auto& breakpoint : breakpoints) {
            if (i + breakpoint.size() <= sentence.size() &&
                sentence.substr(i, breakpoint.size()) == breakpoint) {
                if (!currentChunk.empty()) {
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
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        std::cerr << "Failed to create MuPDF context" << std::endl;
        return;
    }

    fz_register_document_handlers(ctx);

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

        fz_pixmap* pixmap = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), fz_round_rect(bounds), nullptr, 0);
        fz_device* dev = fz_new_draw_device(ctx, fz_identity, pixmap);
        fz_run_page(ctx, page, dev, fz_identity, nullptr);

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

        fz_drop_device(ctx, dev);
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

void PDFTranslator::createPDF(const std::string &output_file, const std::string &input_file, const std::string &images_dir) {
    // Page dimensions in points (A4 size: 612x792 points)
    const int page_width = 612;
    const int page_height = 792;

    // Margins
    const double margin = 72; // 1 inch margins
    const double usable_width = page_width - 2 * margin;

    // Line spacing multiplier
    const double line_spacing = 2.0; // Adjust this value to increase or decrease spacing

    // Create a Cairo surface for the PDF
    cairo_surface_t *surface = cairo_pdf_surface_create(output_file.c_str(), page_width, page_height);
    cairo_t *cr = cairo_create(surface);

    // Get all the file names in the images directory
    std::vector<std::string> image_files;
    for (const auto &entry : std::filesystem::directory_iterator(images_dir)) {
        if (entry.is_regular_file()) {
            image_files.push_back(entry.path().string());
        }
    }
    // Sort the image files to ensure they are added in order 
    std::sort(image_files.begin(), image_files.end());

    // Add images to the PDF
    bool has_images = false; // Track if any images were added
    for (const auto &image_file : image_files) {
        
        // If the file extentsion is not an image, skip it
        std::string extension = std::filesystem::path(image_file).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower); // Convert to lowercase for consistency
        if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".bmp" && extension != ".tiff") {
            std::cout << "Skipping non-image file: " << image_file << std::endl;
            continue; // Skip files that are not supported image types
        }
        
        // Load the image
        cairo_surface_t *image = cairo_image_surface_create_from_png(image_file.c_str());
        if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
            std::cerr << "Failed to load image: " << image_file << std::endl;
            continue;
        }

        // Get image dimensions
        int image_width = cairo_image_surface_get_width(image);
        int image_height = cairo_image_surface_get_height(image);

        // Set the page size to match the image size
        cairo_pdf_surface_set_size(surface, image_width, image_height);

        // Draw the image at (0, 0) since the page size matches the image size
        cairo_save(cr);
        cairo_set_source_surface(cr, image, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);

        // Start a new page for the next image
        cairo_show_page(cr);
        has_images = true; // Mark that at least one image was added

        // Clean up
        cairo_surface_destroy(image);
    }

    // Reset the page size to A4 for text pages only if images were added
    if (has_images) {
        cairo_pdf_surface_set_size(surface, page_width, page_height);
    }

    // Set the font and size for text
    cairo_select_font_face(cr, "Helvetica", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    const double font_size = 12;

    // Set the starting position for text
    double x = margin;
    double y = margin;

    // Open the input file
    std::ifstream infile(input_file);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open file " << input_file << std::endl;
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return;
    }

    // Add text content to the PDF
    std::string line;
    while (std::getline(infile, line)) {
        // Split the line into number and text (assuming the format is number,text)
        size_t comma_pos = line.find(',');
        if (comma_pos != std::string::npos) {
            std::string text = line.substr(comma_pos + 1); // Extract text after the comma

            // Word wrapping logic
            std::istringstream text_stream(text);
            std::string word;
            std::string current_line;
            cairo_text_extents_t extents;

            while (text_stream >> word) {
                // Check if adding the word exceeds the line width
                std::string test_line = current_line.empty() ? word : current_line + " " + word;
                cairo_text_extents(cr, test_line.c_str(), &extents);

                if (extents.width > usable_width) {
                    // Render the current line and move to the next line
                    cairo_move_to(cr, x, y);
                    cairo_show_text(cr, current_line.c_str());
                    y += font_size * line_spacing; // Increase line height by line spacing

                    // Start a new page if necessary
                    if (y > page_height - margin) {
                        cairo_show_page(cr);
                        y = margin; // Reset the y position for the new page
                    }

                    // Start a new line with the current word
                    current_line = word;
                } else {
                    // Add the word to the current line
                    current_line = test_line;
                }
            }

            // Render any remaining text in the current line
            if (!current_line.empty()) {
                cairo_move_to(cr, x, y);
                cairo_show_text(cr, current_line.c_str());
                y += font_size * line_spacing; // Increase line height by line spacing

                // Start a new page if necessary
                if (y > page_height - margin) {
                    cairo_show_page(cr);
                    y = margin;
                }
            }
        }
    }

    // Clean up
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    std::cout << "PDF created with images first: " << output_file << std::endl;
}

size_t PDFTranslator::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

std::string PDFTranslator::uploadDocumentToDeepL(const std::string& filePath, const std::string& deepLKey) {
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

    // Assuming the response contains document_id and document_key, extract them
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