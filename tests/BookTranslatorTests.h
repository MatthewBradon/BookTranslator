#include "EpubTranslator.h"
#include "PDFTranslator.h"
#include "DocxTranslator.h"
#include "GUI.h"
#include <sys/stat.h>


class TestableEpubTranslator : public EpubTranslator {
public:
    using EpubTranslator::searchForOPFFiles;
    using EpubTranslator::getSpineOrder;
    using EpubTranslator::getAllXHTMLFiles;
    using EpubTranslator::sortXHTMLFilesBySpineOrder;
    using EpubTranslator::updateContentOpf;
    using EpubTranslator::cleanChapter;
    using EpubTranslator::extractSpineContent;
    using EpubTranslator::extractIdrefs;
    using EpubTranslator::parseManifestAndSpine;
    using EpubTranslator::updateManifest;
    using EpubTranslator::updateSpine;
    using EpubTranslator::updateNavXHTML;
    using EpubTranslator::copyImages;
    using EpubTranslator::replaceFullWidthSpaces;
    using EpubTranslator::stripHtmlTags;
    using EpubTranslator::readChapterFile;
    using EpubTranslator::writeChapterFile;
    using EpubTranslator::parseHtmlDocument;
    using EpubTranslator::extractNodesFromDoc;
    using EpubTranslator::cleanNodes;
    using EpubTranslator::serializeDocument;
    using EpubTranslator::processImgTag;
    using EpubTranslator::processPTag;
    using EpubTranslator::readFileUtf8;
    using EpubTranslator::extractTags;
    using EpubTranslator::removeSection0001Tags;
    using EpubTranslator::exportEpub;
    using EpubTranslator::removeUnwantedTags;
    using EpubTranslator::containsJapanese;
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
    using PDFTranslator::createMuPDFContext;
    using PDFTranslator::extractTextFromPage;
    using PDFTranslator::pageContainsText;
    using PDFTranslator::extractTextFromBlocks;
    using PDFTranslator::extractTextFromLines;
    using PDFTranslator::extractTextFromChars;
    using PDFTranslator::processPDF;
    using PDFTranslator::initCairoPdfSurface;
    using PDFTranslator::cleanupCairo;
    using PDFTranslator::collectImageFiles;
    using PDFTranslator::isImageFile;
    using PDFTranslator::addImagesToPdf;
    using PDFTranslator::configureTextRendering;
    using PDFTranslator::addTextToPdf;
};

class TestableDocxTranslator : public DocxTranslator {
    public:
        using DocxTranslator::unzip_file;
        using DocxTranslator::make_directory;
        using DocxTranslator::getNodePath;
        using DocxTranslator::extractTextNodesRecursive;
        using DocxTranslator::extractTextNodes;
        using DocxTranslator::saveTextToFile;
        using DocxTranslator::loadTranslations;
        using DocxTranslator::updateNodeWithTranslation;
        using DocxTranslator::traverseAndReinsert;
        using DocxTranslator::reinsertTranslations;
        using DocxTranslator::exportDocx;
        using DocxTranslator::escapeForDocx;
        using DocxTranslator::escapeTranslations;
};