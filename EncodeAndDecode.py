from transformers import AutoTokenizer
import torch
import numpy as np

# Initialize the tokenizer
model_name = 'Helsinki-NLP/opus-mt-ja-en'
tokenizer = AutoTokenizer.from_pretrained(model_name)
print(tokenizer.vocab_size)
        
# Tokenize text and return numpy arrays
def tokenize_text(text):
    inputs = tokenizer(text, return_tensors="pt", padding=True)
    print("Tokenized text test: ", inputs)
    input_ids = inputs['input_ids'].cpu().numpy()  # Convert torch.Tensor to numpy
    attention_mask = inputs['attention_mask'].cpu().numpy()  # Convert torch.Tensor to numpy
        
    return input_ids, attention_mask

# Detokenize text (accepts numpy arrays)
def detokenize_text(text):
    print("\n\n\nOriginal text: ", text)
    if isinstance(text, np.ndarray):
        text = torch.tensor(text)  # Convert numpy array back to torch.Tensor if needed
    decodetext = tokenizer.decode(text, skip_special_tokens=True)
    print("Detokenized text: ", decodetext)
    return decodetext




