from transformers import AutoTokenizer
import torch
import numpy as np
from optimum.onnxruntime import ORTModelForSeq2SeqLM
import multiprocessing as mp

# Global variable to store the model in each worker
model = None

class encodedDataSingleton:
    _instance = None
    _encodedDataDict = None

    @staticmethod
    def get_instance():
        if encodedDataSingleton._instance is None:
            encodedDataSingleton()
        return encodedDataSingleton._instance

    def __init__(self):
        if encodedDataSingleton._instance is not None:
            raise Exception("This class is a singleton!")
        else:
            encodedDataSingleton._instance = self

    def set_encodedDataDict(self, data):
        print("Setting encoded data dict")
        self._encodedDataDict = data

    def get_encodedDataDict(self):
        return self._encodedDataDict





# Initialize tokenizer once (can be shared across workers)
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')

# Function to initialize the model once per worker
def init_model(onnx_model_path):
    global model
    model = ORTModelForSeq2SeqLM.from_pretrained(onnx_model_path)

# Tokenize text and return tensors
def tokenize_text(text):
    inputs = tokenizer(text, return_tensors="pt")
    print("\n\n\n\nOriginal Sentence: ", text)
    print("Tokenized Input: ", inputs)
    return inputs

# Detokenize text (accepts tokenized arrays)
def detokenize_text(text):
    print("Decoder output tokens, ", text)
    decodetext = tokenizer.decode(text, skip_special_tokens=True)
    return decodetext

# Function to run the model and generate translations (used by worker processes)
def run_model_worker(input_encodedDataDict):
    global model
    # Ensure the model has been initialized
    if model is None:
        raise ValueError("Model is not initialized in worker")
    
    generated = model.generate(**input_encodedDataDict)
    return generated[0]

# Function to run the model using multiprocessing
def run_model_multiprocessing(inputs, onnx_model_path, num_workers=4):
    # Create a multiprocessing pool with an initializer to load the model once per worker
    with mp.Pool(processes=num_workers, initializer=init_model, initargs=(onnx_model_path,)) as pool:
        # Prepare data to be distributed across processes
        input_list = [inputs for _ in range(num_workers)]  # Copy the inputs for each worker

        # Run model inference in parallel
        results = pool.map(run_model_worker, input_list)
    
    return results

# Example usage in your C++ application through pybind
def run_translation(text):
    inputs = tokenize_text(text)
    onnx_model_path = 'onnx-model-dir'
    # Run the model with multiprocessing, using 4 workers as an example
    generated_outputs = run_model_multiprocessing(inputs, onnx_model_path, num_workers=4)

    # Detokenize the results from the first model output as an example
    decoded_output = detokenize_text(generated_outputs[0].numpy())
    return decoded_output


def run_model(inputs):
    generated = model.generate(**inputs)
    return generated[0]
