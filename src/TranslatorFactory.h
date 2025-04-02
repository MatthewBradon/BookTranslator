#pragma once
#include "Translator.h"
#include "EpubTranslator.h"
#include "PDFTranslator.h"
#include "DocxTranslator.h"
#include "HTMLTranslator.h"


class TranslatorFactory {
public:
    static std::unique_ptr<Translator> createTranslator(const std::string& translatorType) {
        std::cout << "Creating translator of type: " << translatorType << std::endl;
        if (translatorType == "epub") {
            return std::make_unique<EpubTranslator>();
        } else if (translatorType == "pdf") {
            return std::make_unique<PDFTranslator>();
        } else if (translatorType == "docx") {
            return std::make_unique<DocxTranslator>();
        } else if (translatorType == "html") {
            return std::make_unique<HTMLTranslator>();
        } else {
            throw std::runtime_error("Invalid translator type: " + translatorType);
        }
    }


};

