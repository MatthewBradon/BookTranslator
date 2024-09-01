#include <iostream>
#include <zip.h>
#include <sys/stat.h>    // For Unix-like systems
#ifdef _WIN32
#include <direct.h>      // For Windows
#endif
#include <fstream>
#include <cstring>
#include <filesystem>
#include <string>


std::filesystem::path searchForOPFFiles(const std::filesystem::path& directory) {
    try {
        // Iterate through the directory and its subdirectories
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".opf") {
                return entry.path();
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "General error: " << e.what() << std::endl;
    }

    return nullptr;
}

// Function to create a directory if it doesn't exist
bool create_directory(const std::string& path) {
    struct stat info;
    // std::cout << "Creating directory: " << path << std::endl;
    if (stat(path.c_str(), &info) != 0) {
#ifdef _WIN32
        // Create directory on Windows
        return _mkdir(path.c_str()) == 0;
#else
        // Create directory on Unix-like systems
        return mkdir(path.c_str(), 0755) == 0;
#endif
    } else if (info.st_mode & S_IFDIR) {
        // Directory exists
        return true;
    } else {
        // Path exists but is not a directory
        return false;
    }
}

// Function to unzip a file using libzip
bool unzip_file(const std::string& zipPath, const std::string& outputDir) {
    int err = 0;
    zip *archive = zip_open(zipPath.c_str(), ZIP_RDONLY, &err);
    if (archive == nullptr) {
        std::cerr << "Error opening ZIP archive: " << zipPath << std::endl;
        return false;
    }

    // Get the number of entries (files) in the archive
    zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    for (zip_uint64_t i = 0; i < numEntries; ++i) {
        // Get the file name
        const char* name = zip_get_name(archive, i, 0);
        if (name == nullptr) {
            std::cerr << "Error getting file name in ZIP archive." << std::endl;
            zip_close(archive);
            return false;
        }

        // Open the file inside the ZIP archive
        zip_file* file = zip_fopen_index(archive, i, 0);
        if (file == nullptr) {
            std::cerr << "Error opening file in ZIP archive: " << name << std::endl;
            zip_close(archive);
            return false;
        }

        // Create the full output path for this file
        std::string outputPath = outputDir + "/" + name;

        // Check if the file is in a directory (ends with '/')
        if (name[strlen(name) - 1] == '/') {
            create_directory(outputPath);  // Create directory
        } else {
            // Ensure the directory for the output file exists
            std::string dirPath = outputPath.substr(0, outputPath.find_last_of('/'));
            create_directory(dirPath);

            // Open the output file
            std::ofstream outputFile(outputPath, std::ios::binary);
            if (!outputFile.is_open()) {
                std::cerr << "Error creating output file: " << outputPath << std::endl;
                zip_fclose(file);
                zip_close(archive);
                return false;
            }

            // Read from the ZIP file and write to the output file
            char buffer[4096];
            zip_int64_t bytesRead;
            while ((bytesRead = zip_fread(file, buffer, sizeof(buffer))) > 0) {
                outputFile.write(buffer, bytesRead);
            }

            outputFile.close();
        }

        // Close the current file in the ZIP archive
        zip_fclose(file);
    }

    // Close the ZIP archive
    zip_close(archive);

    return true;
}

int main() {
    std::string epubToConvert = "rawEpub/この素晴らしい世界に祝福を！ 01　あぁ、駄女神さま.epub";
    std::string unzippedPath = "unzipped/";

    std::string templatePath = "export/";
    std::string templateEpub = "rawEpub/template.epub";

    // Create the output directory if it doesn't exist
    if (!create_directory(unzippedPath)) {
        std::cerr << "Failed to create output directory: " << unzippedPath << std::endl;
        return 1;
    }

    // Unzip the EPUB file
    if (!unzip_file(epubToConvert, unzippedPath)) {
        std::cerr << "Failed to unzip EPUB file: " << epubToConvert << std::endl;
        return 1;
    }

    std::cout << "EPUB file unzipped successfully to: " << unzippedPath << std::endl;

    // Create the template directory if it doesn't exist
    if (!create_directory(templatePath)) {
        std::cerr << "Failed to create template directory: " << templatePath << std::endl;
        return 1;
    }

    // Unzip the template EPUB file
    if (!unzip_file(templateEpub, templatePath)) {
        std::cerr << "Failed to unzip EPUB file: " << templateEpub << std::endl;
        return 1;
    }

    std::cout << "EPUB file unzipped successfully to: " << templatePath << std::endl;

    

    std::filesystem::path contentOpf = searchForOPFFiles(std::filesystem::path(unzippedPath));

    if (contentOpf.empty()) {
        std::cerr << "No OPF file found in the unzipped EPUB directory." << std::endl;
        return 1;
    }

    std::cout << "Found OPF file: " << contentOpf << std::endl;



    return 0;
}
