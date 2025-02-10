# Book Translator
A Japanese to English Book Translator application using local AI models. 

All the packages except MUPDF are installed and managed by VCPKG

MUPDF is included as a submodule for this repository after cloning run this command to have all of the submodules so you can build MUPDF
```
git submodule update --init --recursive
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
To export the fine tuned model with past key values use this command
```
optimum-cli export onnx --model ./fine_tuned_model ./onnx-model-dir --task text2text-generation-with-past
```



When running pyinstaller either have all the python modules installed already or run it while in a venv


For testing run
```
ctest -V -C Release --test-dir build
```



# Evaluation Notes

For notes the fine-tuned model was trained on NilanE/ParallelFiction-Ja_En-100k for 3 epochs

Evaluation data was sampled from the test split
```
test_data = test_data.shuffle(seed=42).select(range(100))
```


The Meteor score ranges from 0 to 1
The BLEU Score ranges from 0 to 100

Both model are evaluated on unseen parts of NilanE/ParallelFiction-Ja_En-100k

1. Num beams 4
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.37
        - METEOR Score: 0.02

    - Fined tuned model
        - BLEU Score: 54.29
        - METEOR Score: 0.13

2. No beams but with length_penalty=1.2 and no_repeat_ngram_size=3 to stop repition
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.36
        - METEOR Score: 0.03

    - Fined tuned model
        - BLEU Score: 44.86
        - METEOR Score: 0.13

3. Num beams = 4 with length_penalty=0.6 and no_repeat_ngram_size=3 to stop repition and early stopping
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.34
        - METEOR Score: 0.02

    - Fined tuned model
        - BLEU Score: 53.63
        - METEOR Score: 0.14

4. Num beams = 4 with length_penalty=1.2 and no_repeat_ngram_size=3 to stop repition and early stopping
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.34
        - METEOR Score: 0.02

    - Fined tuned model
        - BLEU Score: 53.63
        - METEOR Score: 0.14