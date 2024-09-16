from transformers import MarianTokenizer, MarianMTModel
import torch
import numpy as np

# Initialize the tokenizer
model_name = 'Helsinki-NLP/opus-mt-ja-en'
tokenizer = MarianTokenizer.from_pretrained(model_name)
        
# Tokenize text and return numpy arrays
def tokenize_text(text):
    inputs = tokenizer(text, return_tensors="pt", padding=True)
    print("Tokenized text test: ", inputs)
    input_ids = inputs['input_ids'].cpu().numpy()  # Convert torch.Tensor to numpy
    attention_mask = inputs['attention_mask'].cpu().numpy()  # Convert torch.Tensor to numpy
    
    print("Bos token ID: ", tokenizer.bos_token_id)
    
    return input_ids, attention_mask

# Detokenize text (accepts numpy arrays)
def detokenize_text(text):
    print("\n\n\nOriginal text: ", text)
    if isinstance(text, np.ndarray):
        text = torch.tensor(text)  # Convert numpy array back to torch.Tensor if needed
    decodetext = tokenizer.decode(text, skip_special_tokens=True)
    print("Detokenized text: ", decodetext)
    return decodetext

def get_bos_token_id():
    bos_token_id = tokenizer.bos_token_id
    if bos_token_id is None:
        # Fallback to 0 or any default token, depending on your model's needs
        bos_token_id = 0
        
    print("BOS token ID: ", bos_token_id)
    return bos_token_id


def get_eos_token_id():
    eos_token_id = tokenizer.eos_token_id
    if eos_token_id is None:
        # Fallback to 0 or any default token, depending on your model's needs
        eos_token_id = 0
        
    print("EOS token ID: ", eos_token_id)
    return eos_token_id


# model = MarianMTModel.from_pretrained(model_name)
# # Move the model to CPU (ONNX export usually happens on CPU)
# device = torch.device("cpu")
# model.to(device)


# def translate_text(text):
    
#     print("\n\n\nOriginal text: ", text)
#     with torch.no_grad():
#             inputs = tokenizer(text, return_tensors="pt", padding=True).to(device)
#             translated = model.generate(**inputs)
#             decodetext = tokenizer.decode(translated[0], skip_special_tokens=True)
            
            
            
#             print("Translated text: ", decodetext)
            
#             return decodetext
        
        
# translate_text("私は猫です。")