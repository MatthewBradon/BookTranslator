# Book Translator
A Japanese to English Book Translator application using local AI models. 


I built this using cmake and the Visaul Studio Community 2022 Build Tools

```
cmake --build build --config Release
```

All the packages except MUPDF are installed and managed by VCPKG

If using VSCode make sure to include in your settings
```
    "cmake.configureArgs": ["-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"],
``


The following commands need to be ran as they are runtime dependencies for the application:

For the translation and tokenizing EpubTranslator.cpp and PDFTranslator.cpp uses standalone executable files to create them uses these commands
```
pyinstaller --onefile --name multiprocessTranslation --distpath ./ ./multiprocessTranslation.py 
```
```
pyinstaller --onefile --name tokenizeRawTags --distpath ./ ./tokenizeRawTags.py
```

To create the AI model use optimum-cli to export the model to the ONNX format and to the onnx-model-dir
```
optimum-cli export onnx --model Helsinki-NLP/opus-mt-ja-en ./onnx-model-dir
```

