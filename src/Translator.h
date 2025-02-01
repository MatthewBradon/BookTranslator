#pragma once

#include <string>

class Translator {
public:
    virtual ~Translator() = default;

    // Pure virtual method to be implemented by derived classes
    virtual int run(const std::string& inputPath, const std::string& outputPath, int localModel, const std::string& deepLKey) = 0;
};
