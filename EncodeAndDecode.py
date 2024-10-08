from transformers import AutoTokenizer
import torch
import numpy as np
from optimum.onnxruntime import ORTModelForSeq2SeqLM

tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')
onnxModelPath = 'onnx-model-dir'
model = ORTModelForSeq2SeqLM.from_pretrained(onnxModelPath)

# Tokenize text and return numpy arrays
def tokenize_text(text):
    inputs = tokenizer(text, return_tensors="pt")
    print("\n\n\n\nOriginal Sentence: ", text)
    print("Tokenized Input: ", inputs)
    return inputs


# Detokenize text (accepts numpy arrays)
def detokenize_text(text):
    print("Decoder output tokens, ", text)
    decodetext = tokenizer.decode(text, skip_special_tokens=True)
    return decodetext

def run_model(inputs):
    generated = model.generate(**inputs)
    return generated

def translate(text):
    inputs = tokenizer(text, return_tensors='pt')
    generated = model.generate(**inputs)
    return tokenizer.decode(generated[0], skip_special_tokens=True)
