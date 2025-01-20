#pragma once
#include "Translator.h"
#include "EpubTranslator.h"
#include "PDFTranslator.h"




class TranslatorFactory {
public:
    static std::shared_ptr<Translator> createTranslator(const std::string& translatorType) {
        if (translatorType == "epub") {
            return std::make_shared<EpubTranslator>();
        } else if (translatorType == "pdf") {
            return std::make_shared<PDFTranslator>();
        } else {
            throw std::runtime_error("Invalid translator type: " + translatorType);
        }
    }


};

