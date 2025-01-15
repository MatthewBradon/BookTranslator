# Epub Translator
A Japanese to English Epub Translator application using local AI models
```
cmake --build build --config Release
```

If using VSCode make sure to include in your settings
```
    "cmake.configureArgs": ["-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"],
``

For the translation and tokenizing main.cpp uses standalone .exe files to create them uses these commands
```
pyinstaller --onefile --name multiprocessTranslation --distpath ./ ./multiprocessTranslation.py 
```
```
pyinstaller --onefile --name tokenizeRawTags --distpath ./ .\tokenizeRawTags.py
```

To create the ONNX model make sure you have optimum-cli installed and run this command
```
optimum-cli export onnx --model Helsinki-NLP/opus-mt-ja-en ./onnx-model-dir
```