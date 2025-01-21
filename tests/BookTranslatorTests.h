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
