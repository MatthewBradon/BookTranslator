# Book Translator
A Japanese to English Book Translator application using local AI models. 

All the packages except MUPDF are installed and managed by VCPKG

MUPDF is included as a submodule for this repository so when cloning run this command:
```
git clone --recurse-submodules https://github.com/MatthewBradon/EpubTranslator.git
```


I built this using cmake and the Visaul Studio Community 2022 Build Tools

```
cmake --build build --config Release
```

If using VSCode make sure to include in your settings
```
    "cmake.configureArgs": ["-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"],
```


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

