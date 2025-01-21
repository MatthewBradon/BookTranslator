#include "EpubTranslator.h"
#include "PDFTranslator.h"
#include "GUI.h"


class TestableEpubTranslator : public EpubTranslator {
public:
    using EpubTranslator::searchForOPFFiles;
    using EpubTranslator::getSpineOrder;
    using EpubTranslator::getAllXHTMLFiles;
    using EpubTranslator::sortXHTMLFilesBySpineOrder;
    using EpubTranslator::updateContentOpf;
    using EpubTranslator::cleanChapter;
};

class TestableGUI : public GUI {
public:
    using GUI::inputFile;
    using GUI::outputPath;
    using GUI::workerThread;
    using GUI::running;
    using GUI::finished;
    using GUI::resultMutex;
    using GUI::statusMessage;
    using GUI::result;
};

class TestablePDFTranslator : public PDFTranslator {
public:
    using PDFTranslator::removeWhitespace;
    using PDFTranslator::extractTextFromPDF;
    using PDFTranslator::processAndSplitText;
    using PDFTranslator::splitLongSentences;
    using PDFTranslator::splitJapaneseText;
    using PDFTranslator::getUtf8CharLength;
    using PDFTranslator::convertPdfToImages;
    using PDFTranslator::isImageAboveThreshold;
    using PDFTranslator::createPDF;
};