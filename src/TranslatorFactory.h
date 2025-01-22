#pragma once
#include "Translator.h"
#include "EpubTranslator.h"
#include "PDFTranslator.h"




class TranslatorFactory {
public:
    static std::unique_ptr<Translator> createTranslator(const std::string& translatorType) {
        if (translatorType == "epub") {
            return std::make_unique<EpubTranslator>();
        } else if (translatorType == "pdf") {
            return std::make_unique<PDFTranslator>();
        } else {
            throw std::runtime_error("Invalid translator type: " + translatorType);
        }
    }


};

