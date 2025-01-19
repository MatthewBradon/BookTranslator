#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "PDFParser.h"

int PDFParser::run() {
    // Check if the main temp files exist and delete them
    std::string rawTextFilePath = "pdftext.txt";
    const std::string extractedTextPath = "extractedPDFtext.txt";
    std::string imagesDir = "FilteredImages";

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

    // if (std::filesystem::exists("translatedText.txt")) {
    //     std::filesystem::remove("translatedText.txt");
    // }

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
    std::cout << "Hello from PDFParser!\n";

    std::string pdfFilePath = "C:/Users/matth/Desktop/Kono Subarashii Sekai ni Shukufuku wo! [JP]/Konosuba Volume 1 [JP].pdf";
    // std::string pdfFilePath = "/Users/xsmoked/Downloads/Konosuba Volume 1 [JP].pdf";


    // Convert the PDF to images
    convertPdfToImages(pdfFilePath, imagesDir, 50.0f);

    std::cout << "Finished converting PDF to images" << '\n';

    // Extracts text from the PDF and writes it to extractedPDFtext.txt
    extractTextFromPDF(pdfFilePath, extractedTextPath);

    // Splits the merged japanese into sentence and writes it to pdftext.txt
    try {
        processAndSplitText(extractedTextPath, rawTextFilePath, 300);
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

    // std::string multiprocessExePath = multiprocessExe.string();
    

    // std::cout << "Before call to multiprocessTranslation.py" << '\n';

    // boost::process::ipstream pipe_stdout, pipe_stderr;

    // try {

    //     boost::process::child c(
    //         multiprocessExePath,
    //         chapterNumberMode,
    //         boost::process::std_out > pipe_stdout, 
    //         boost::process::std_err > pipe_stderr
    //     );

    //     // Threads to handle asynchronous reading
    //     std::thread stdout_thread([&pipe_stdout]() {
    //         std::string line;
    //         while (std::getline(pipe_stdout, line)) {
    //             std::cout << "Python stdout: " << line << "\n";
    //         }
    //     });

    //     std::thread stderr_thread([&pipe_stderr]() {
    //         std::string line;
    //         while (std::getline(pipe_stderr, line)) {
    //             std::cerr << "Python stderr: " << line << "\n";
    //         }
    //     });

    //     c.wait(); // Wait for process completion

    //     stdout_thread.join();
    //     stderr_thread.join();

    //     if (c.exit_code() == 0) {
    //         std::cout << "Translation Python script executed successfully." << "\n";
    //     } else {
    //         std::cerr << "Translation Python script exited with code: " << c.exit_code() << "\n";
    //     }
    // } catch (const std::exception& ex) {
    //     std::cerr << "Exception: " << ex.what() << "\n";
    // }

    // std::cout << "After call to multiprocessTranslation.py" << '\n';

    // Create the final PDF
    std::string outputPdfPath = "output.pdf";
    

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

void PDFParser::extractTextFromPDF(const std::string& pdfFilePath, const std::string& outputFilePath) {

    std::cout << "Extracting text from PDF..." << std::endl;
    std::cout << "PDF file: " << pdfFilePath << std::endl;
    std::cout << "Output file: " << outputFilePath << std::endl;

    auto startTime = std::chrono::high_resolution_clock::now();


    // Check if the file exists
    if (!std::filesystem::exists(pdfFilePath)) {
        throw std::runtime_error("PDF file does not exist: " + pdfFilePath);
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
        fz_document *doc = fz_open_document(ctx, pdfFilePath.c_str());
        if (!doc) {
            throw std::runtime_error("Failed to open PDF file: " + pdfFilePath);
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

void PDFParser::processAndSplitText(const std::string& inputFilePath, const std::string& outputFilePath, size_t maxLength) {
    // Open the input file
    std::ifstream inputFile(inputFilePath);
    if (!inputFile.is_open()) {
        throw std::runtime_error("Failed to open input file: " + inputFilePath);
    }

    // Read the entire file content into a string
    std::string text((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
    inputFile.close();

    // Process and split the text into sentences
    std::vector<std::string> sentences = splitJapaneseText(text, maxLength);

    // Open the output file
    std::ofstream outputFile(outputFilePath);
    if (!outputFile.is_open()) {
        throw std::runtime_error("Failed to open output file: " + outputFilePath);
    }

    // Write each sentence to the file with a corresponding number
    for (size_t i = 0; i < sentences.size(); ++i) {
        outputFile << (i + 1) << "," << sentences[i] << "\n";
    }

    outputFile.close();
}

void PDFParser::convertPdfToImages(const std::string &pdfPath, const std::string &outputFolder, float stdDevThreshold) {
    // Create MuPDF context
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        std::cerr << "Failed to create MuPDF context" << std::endl;
        return;
    }

    // Enable anti-aliasing for better rendering quality
    fz_set_aa_level(ctx, 8);

    // Register document handlers
    fz_register_document_handlers(ctx);

    // Open the PDF document
    fz_document* doc = fz_open_document(ctx, pdfPath.c_str());
    if (!doc) {
        std::cerr << "Failed to open PDF document: " << pdfPath << std::endl;
        fz_drop_context(ctx);
        return;
    }

    // Create output directory
    std::filesystem::create_directory(outputFolder);

    // Get the number of pages in the document
    int pageCount = fz_count_pages(ctx, doc);

    for (int pageNum = 0; pageNum < pageCount; pageNum++) {
        // Load the page
        fz_page* page = fz_load_page(ctx, doc, pageNum);

        // Extract page bounds
        fz_rect bounds = fz_bound_page(ctx, page);

        // Render page to pixmap at default resolution (72 DPI)
        fz_pixmap* pixmap = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), fz_round_rect(bounds), nullptr, 0);
        fz_clear_pixmap_with_value(ctx, pixmap, 255); // Set background to white

        fz_device* dev = fz_new_draw_device(ctx, fz_identity, pixmap);
        fz_run_page(ctx, page, dev, fz_identity, nullptr);

        // Save the rendered image temporarily
        char tempPath[1024];
        snprintf(tempPath, sizeof(tempPath), "%s/temp_page_%03d.png", outputFolder.c_str(), pageNum + 1);
        fz_save_pixmap_as_png(ctx, pixmap, tempPath);

        // Filter the image based on standard deviation
        if (isImageAboveThreshold(tempPath, stdDevThreshold)) {
            // Rescale the image to high resolution (300 DPI)
            fz_matrix transform = fz_scale(300.0 / 72.0, 300.0 / 72.0); // Scale to 300 DPI
            fz_transform_rect(bounds, transform);

            fz_pixmap* highResPixmap = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), fz_round_rect(bounds), nullptr, 0);
            fz_clear_pixmap_with_value(ctx, highResPixmap, 255); // Set background to white

            fz_device* highResDev = fz_new_draw_device(ctx, transform, highResPixmap);
            fz_run_page(ctx, page, highResDev, transform, nullptr);

            // Save the high-resolution image
            char outputPath[1024];
            snprintf(outputPath, sizeof(outputPath), "%s/page_%03d.png", outputFolder.c_str(), pageNum + 1);
            fz_save_pixmap_as_png(ctx, highResPixmap, outputPath);
            std::cout << "Saved high-resolution image: " << outputPath << std::endl;

            // Clean up high-resolution resources
            fz_drop_device(ctx, highResDev);
            fz_drop_pixmap(ctx, highResPixmap);
        } else {
            // Delete the temporary image if it doesn't pass the filter
            std::filesystem::remove(tempPath);
        }

        // Clean up resources for the current page
        fz_drop_device(ctx, dev);
        fz_drop_pixmap(ctx, pixmap);
        fz_drop_page(ctx, page);
    }

    // Clean up MuPDF resources
    fz_drop_document(ctx, doc);
    fz_drop_context(ctx);
}


bool PDFParser::isImageAboveThreshold(const std::string &imagePath, float threshold) {
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

void PDFParser::createPDF(const std::string &output_file, const std::string &input_file, const std::string &images_dir) {
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

    // Add images to the PDF
    bool is_first_page = true; // Track whether we're on the first page
    for (const auto &entry : std::filesystem::directory_iterator(images_dir)) {
        if (entry.is_regular_file()) {
            // Load the image
            cairo_surface_t *image = cairo_image_surface_create_from_png(entry.path().string().c_str());
            if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
                std::cerr << "Failed to load image: " << entry.path() << std::endl;
                continue;
            }

            // Get image dimensions
            int image_width = cairo_image_surface_get_width(image);
            int image_height = cairo_image_surface_get_height(image);

            // Scale the image to fit the page
            double scale_x = static_cast<double>(page_width) / image_width;
            double scale_y = static_cast<double>(page_height) / image_height;
            double scale = std::min(scale_x, scale_y);

            // Center the image
            double offset_x = (page_width - image_width * scale) / 2;
            double offset_y = (page_height - image_height * scale) / 2;

            // If it's not the first page, explicitly start a new page
            if (!is_first_page) {
                cairo_show_page(cr);
            } else {
                is_first_page = false; // First page is now used
            }

            // Draw the image
            cairo_save(cr);
            cairo_translate(cr, offset_x, offset_y);
            cairo_scale(cr, scale, scale);
            cairo_set_source_surface(cr, image, 0, 0);
            cairo_paint(cr);
            cairo_restore(cr);

            // Clean up
            cairo_surface_destroy(image);
        }
    }

    // Start a new page after the images to prevent overlap with text
    cairo_show_page(cr);

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

    // Finalize the drawing
    cairo_show_page(cr);

    // Clean up
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    std::cout << "PDF created with images first: " << output_file << std::endl;
}