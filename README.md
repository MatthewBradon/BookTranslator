# Book Translator
A Japanese to English Book Translator application using local AI models. 

All the packages except MUPDF are installed and managed by VCPKG

**PDFTranslator does not support OCR and only works with text based PDFs.**

MUPDF is included as a submodule for this repository after cloning run this command to have all of the submodules so you can build MUPDF
```
git submodule update --init --recursive
```



I built this using cmake and the Visual Studio Community 2022 Build Tools

```
cmake --build build --config Release
```

If using VSCode make sure to include in your settings
```
    "cmake.configureArgs": ["-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"],
```

If on windows and want to statically link make sure VCPKG gets the static triplet
```
    "cmake.configureArgs": [
        "-DCMAKE_TOOLCHAIN_FILE=C:/Users/Matt/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "-DVCPKG_TARGET_TRIPLET=x64-windows-static"
    ],
```



The following commands need to be ran as they are runtime dependencies for the application:

For the translation and tokenizing EpubTranslator.cpp and PDFTranslator.cpp uses standalone executable files to create them uses these commands
```
pyinstaller --onefile --name translation --distpath ./ ./translation.py
```

To create the AI model use optimum-cli to export the model to the ONNX format and to the onnx-model-dir
```
optimum-cli export onnx --model Helsinki-NLP/opus-mt-mul-en ./onnx-model-dir
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

If you wish to change some of the model parameters while generating change the values in the `translationConfig.json`



If you are fine-tuning the model and want to use CUDA I recommend making a conda environment and installing the following packages:
```
pip3 install torch --index-url https://download.pytorch.org/whl/cu126
pip install transformers datasets sacrebleu nltk sentencepiece transformers[torch]
```


# Evaluation Notes

For notes the fine-tuned model was trained on NilanE/ParallelFiction-Ja_En-100k for 3 epochs

Evaluation data was sampled from the test split
```
test_data = test_data.shuffle(seed=42).select(range(100))
```
## Helsinki-NLP/opus-mt-ja-en

The Meteor score ranges from 0 to 1
The BLEU Score ranges from 0 to 100

Both model are evaluated on unseen parts of NilanE/ParallelFiction-Ja_En-100k

max_new_tokens=512, num_beams=2, do_sample=True, top_k=50, top_p=0.9, temperature=1.0
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 1.97645
        - METEOR Score: 0.02417

    - Fine-tuned Model
        - BLEU Score: 53.37589
        - METEOR Score: 0.13356

==================================================

max_new_tokens=128, num_beams=4, early_stopping=True
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.05157
        - METEOR Score: 0.01912

    - Fine-tuned Model
        - BLEU Score: 54.28504
        - METEOR Score: 0.12801

==================================================

max_new_tokens=256, num_beams=4, no_repeat_ngram_size=3, repetition_penalty=1.2, length_penalty=1.0
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.00004
        - METEOR Score: 0.01936

    - Fine-tuned Model
        - BLEU Score: 52.40971
        - METEOR Score: 0.14338

==================================================

max_new_tokens=256, num_beams=5, no_repeat_ngram_size=3, repetition_penalty=1.1, temperature=0.8
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.00007
        - METEOR Score: 0.01957

    - Fine-tuned Model
        - BLEU Score: 52.54894
        - METEOR Score: 0.14307

==================================================

max_new_tokens=512, num_beams=6, no_repeat_ngram_size=4, repetition_penalty=1.2, temperature=0, length_penalty=2.0
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 17.90680
        - METEOR Score: 0.06080

    - Fine-tuned Model
        - BLEU Score: 52.60082
        - METEOR Score: 0.14347

==================================================

max_new_tokens=256, num_beams=3, do_sample=True, temperature=1.2, top_p=0.7, top_k=100, early_stopping=True, length_penalty=1.1
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 3.35249
        - METEOR Score: 0.02379

    - Fine-tuned Model
        - BLEU Score: 54.75590
        - METEOR Score: 0.13160

==================================================

max_new_tokens=256, num_beams=1, do_sample=True, top_p=1.0, top_k=0, temperature=1.5, repetition_penalty=1.0
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 1.56923
        - METEOR Score: 0.01296

    - Fine-tuned Model
        - BLEU Score: 1.20216
        - METEOR Score: 0.04434

==================================================

max_new_tokens=300, num_beams=5, no_repeat_ngram_size=3, repetition_penalty=1.3, length_penalty=1.5
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 2.43477
        - METEOR Score: 0.04277

    - Fine-tuned Model
        - BLEU Score: 52.40892
        - METEOR Score: 0.14300

==================================================

max_new_tokens=400, num_beams=4, no_repeat_ngram_size=2, early_stopping=True, temperature=0.5, repetition_penalty=1.05
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.00995
        - METEOR Score: 0.01774

    - Fine-tuned Model
        - BLEU Score: 47.58037
        - METEOR Score: 0.13732

==================================================

max_new_tokens=350, num_beams=5, no_repeat_ngram_size=3, repetition_penalty=1.2, do_sample=True, top_k=40, top_p=0.85, temperature=1.1
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.92960
        - METEOR Score: 0.02131

    - Fine-tuned Model
        - BLEU Score: 52.62736
        - METEOR Score: 0.14214

==================================================

max_new_tokens=512, num_beams=8, no_repeat_ngram_size=4, repetition_penalty=1.0, length_penalty=1.2, temperature=0.3, early_stopping=False
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.48515
        - METEOR Score: 0.03120

    - Fine-tuned Model
        - BLEU Score: 53.16295
        - METEOR Score: 0.14225

==================================================

max_new_tokens=128, num_beams=5, length_penalty=0.8, no_repeat_ngram_size=2, do_sample=False, temperature=0.0
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.00006
        - METEOR Score: 0.01391

    - Fine-tuned Model
        - BLEU Score: 48.74128
        - METEOR Score: 0.13906

==================================================

max_new_tokens=512, num_beams=4, no_repeat_ngram_size=3, repetition_penalty=1.3, length_penalty=2.0, temperature=1.0, early_stopping=True
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.01750
        - METEOR Score: 0.02139

    - Fine-tuned Model
        - BLEU Score: 49.89703
        - METEOR Score: 0.14339

==================================================

max_new_tokens=256, num_beams=2, no_repeat_ngram_size=2, repetition_penalty=1.0, do_sample=True, top_k=50, temperature=1.3, top_p=0.9
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.54719
        - METEOR Score: 0.02226

    - Fine-tuned Model
        - BLEU Score: 42.20732
        - METEOR Score: 0.13531

==================================================

max_new_tokens=256, num_beams=8, no_repeat_ngram_size=4, repetition_penalty=1.1, length_penalty=1.0, temperature=0.6, top_p=0.95, do_sample=True
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.00078
        - METEOR Score: 0.02233

    - Fine-tuned Model
        - BLEU Score: 56.72177
        - METEOR Score: 0.14304

==================================================

max_new_tokens=400, num_beams=5, no_repeat_ngram_size=3, repetition_penalty=1.25, temperature=0.2, top_p=0.9, top_k=40, early_stopping=False
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.00021
        - METEOR Score: 0.01984

    - Fine-tuned Model
        - BLEU Score: 52.63562
        - METEOR Score: 0.14279

==================================================

max_new_tokens=512, num_beams=6, no_repeat_ngram_size=2, repetition_penalty=1.05, length_penalty=2.0, temperature=0.9, do_sample=False
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 29.91367
        - METEOR Score: 0.07786

    - Fine-tuned Model
        - BLEU Score: 46.07286
        - METEOR Score: 0.13885

==================================================

max_new_tokens=512, num_beams=4, no_repeat_ngram_size=3, repetition_penalty=1.3, temperature=1.4, top_k=100, top_p=0.8, length_penalty=1.3
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 3.95208
        - METEOR Score: 0.03001

    - Fine-tuned Model
        - BLEU Score: 49.89703
        - METEOR Score: 0.14276

==================================================

max_new_tokens=200, num_beams=3, no_repeat_ngram_size=3, repetition_penalty=1.2, temperature=0.7, do_sample=True, top_p=0.9, top_k=50
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 0.01581
        - METEOR Score: 0.02203

    - Fine-tuned Model
        - BLEU Score: 52.77237
        - METEOR Score: 0.14248

==================================================

max_new_tokens=512, num_beams=5, no_repeat_ngram_size=5, repetition_penalty=1.3, length_penalty=1.0, temperature=0.0
    - Helsinki-NLP/opus-mt-ja-en
        - BLEU Score: 1.88293
        - METEOR Score: 0.02017

    - Fine-tuned Model
        - BLEU Score: 53.10960
        - METEOR Score: 0.14308

==================================================




## Helsinki-NLP/opus-mt-mul-en (Fine tuning on Japanese)

The Helsinki-NLP/opus-mt-mul-en mutiple supports a wide array of languages as its source language to English.

The model was fine-tuned for Japanese to English specifically with NilanE/ParallelFiction-Ja_En-100k for 3 epochs

`>>jpn<<` language token was added on each example to specify the source language

Hlesinki-NLP/opus-mt-mul-en vs Fine-tuned Model

max_new_tokens=512, num_beams=2, do_sample=True, top_k=50, top_p=0.9, temperature=1.0
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 48.70602
        - METEOR Score: 0.00881

    - Fine-tuned Model
        - BLEU Score: 52.66190
        - METEOR Score: 0.11475

==================================================

max_new_tokens=128, num_beams=4, early_stopping=True
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00473

    - Fine-tuned Model
        - BLEU Score: 20.90445
        - METEOR Score: 0.10118

==================================================

max_new_tokens=256, num_beams=4, no_repeat_ngram_size=3, repetition_penalty=1.2, length_penalty=1.0
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00727

    - Fine-tuned Model
        - BLEU Score: 52.40740
        - METEOR Score: 0.12540

==================================================

max_new_tokens=256, num_beams=5, no_repeat_ngram_size=3, repetition_penalty=1.1, temperature=0.8
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00782

    - Fine-tuned Model
        - BLEU Score: 51.29767
        - METEOR Score: 0.12649

==================================================

max_new_tokens=512, num_beams=6, no_repeat_ngram_size=4, repetition_penalty=1.2, temperature=0, length_penalty=2.0
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.03434
        - METEOR Score: 0.04225

    - Fine-tuned Model
        - BLEU Score: 57.39370
        - METEOR Score: 0.12379

==================================================

max_new_tokens=256, num_beams=3, do_sample=True, temperature=1.2, top_p=0.7, top_k=100, early_stopping=True, length_penalty=1.1
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00758

    - Fine-tuned Model
        - BLEU Score: 19.37795
        - METEOR Score: 0.11442

==================================================

max_new_tokens=256, num_beams=1, do_sample=True, top_p=1.0, top_k=0, temperature=1.5, repetition_penalty=1.0
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 4.47854
        - METEOR Score: 0.00699

    - Fine-tuned Model
        - BLEU Score: 4.20067
        - METEOR Score: 0.04515

==================================================

max_new_tokens=300, num_beams=5, no_repeat_ngram_size=3, repetition_penalty=1.3, length_penalty=1.5
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.74938
        - METEOR Score: 0.01813

    - Fine-tuned Model
        - BLEU Score: 53.21915
        - METEOR Score: 0.12435

==================================================

max_new_tokens=400, num_beams=4, no_repeat_ngram_size=2, early_stopping=True, temperature=0.5, repetition_penalty=1.05
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00644

    - Fine-tuned Model
        - BLEU Score: 48.98365
        - METEOR Score: 0.13186

==================================================

max_new_tokens=350, num_beams=5, no_repeat_ngram_size=3, repetition_penalty=1.2, do_sample=True, top_k=40, top_p=0.85, temperature=1.1
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00061
        - METEOR Score: 0.00837

    - Fine-tuned Model
        - BLEU Score: 52.46137
        - METEOR Score: 0.12528

==================================================

max_new_tokens=512, num_beams=8, no_repeat_ngram_size=4, repetition_penalty=1.0, length_penalty=1.2, temperature=0.3, early_stopping=False
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00011
        - METEOR Score: 0.01656

    - Fine-tuned Model
        - BLEU Score: 52.66559
        - METEOR Score: 0.12465

==================================================

max_new_tokens=128, num_beams=5, length_penalty=0.8, no_repeat_ngram_size=2, do_sample=False, temperature=0.0
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00545

    - Fine-tuned Model
        - BLEU Score: 51.30991
        - METEOR Score: 0.13201

==================================================

max_new_tokens=512, num_beams=4, no_repeat_ngram_size=3, repetition_penalty=1.3, length_penalty=2.0, temperature=1.0, early_stopping=True
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00763

    - Fine-tuned Model
        - BLEU Score: 54.61151
        - METEOR Score: 0.12550

==================================================

max_new_tokens=256, num_beams=2, no_repeat_ngram_size=2, repetition_penalty=1.0, do_sample=True, top_k=50, temperature=1.3, top_p=0.9
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00932

    - Fine-tuned Model
        - BLEU Score: 43.58065
        - METEOR Score: 0.12685

==================================================

max_new_tokens=256, num_beams=8, no_repeat_ngram_size=4, repetition_penalty=1.1, length_penalty=1.0, temperature=0.6, top_p=0.95, do_sample=True
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00867

    - Fine-tuned Model
        - BLEU Score: 55.42347
        - METEOR Score: 0.12291

==================================================

max_new_tokens=400, num_beams=5, no_repeat_ngram_size=3, repetition_penalty=1.25, temperature=0.2, top_p=0.9, top_k=40, early_stopping=False
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00783

    - Fine-tuned Model
        - BLEU Score: 54.03300
        - METEOR Score: 0.12480

==================================================

max_new_tokens=512, num_beams=6, no_repeat_ngram_size=2, repetition_penalty=1.05, length_penalty=2.0, temperature=0.9, do_sample=False
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.02157
        - METEOR Score: 0.04098

    - Fine-tuned Model
        - BLEU Score: 48.53855
        - METEOR Score: 0.13312

==================================================

max_new_tokens=512, num_beams=4, no_repeat_ngram_size=3, repetition_penalty=1.3, temperature=1.4, top_k=100, top_p=0.8, length_penalty=1.3
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.04122
        - METEOR Score: 0.01247

    - Fine-tuned Model
        - BLEU Score: 54.61151
        - METEOR Score: 0.12550

==================================================

max_new_tokens=200, num_beams=3, no_repeat_ngram_size=3, repetition_penalty=1.2, temperature=0.7, do_sample=True, top_p=0.9, top_k=50
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00819

    - Fine-tuned Model
        - BLEU Score: 51.12672
        - METEOR Score: 0.12615

==================================================

max_new_tokens=512, num_beams=5, no_repeat_ngram_size=5, repetition_penalty=1.3, length_penalty=1.0, temperature=0.0
    - Helsinki-NLP/opus-mt-mul-en
        - BLEU Score: 0.00000
        - METEOR Score: 0.00750

    - Fine-tuned Model
        - BLEU Score: 53.54939
        - METEOR Score: 0.12182

==================================================

