import torch
import numpy as np
from transformers import AutoTokenizer
from onnxruntime import InferenceSession


print(torch.cuda.is_available())  # Should return True if CUDA is available


# Path to the directory containing the ONNX models and config files
model_path = 'onnx-model-dir'

# Load the tokenizer
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')

# Tokenize the text
text = 'こんにちは、元気ですか？'
inputs = tokenizer(text, return_tensors='pt')

# Load the ONNX models
encoder_path = 'onnx-model-dir/encoder_model.onnx'
decoder_path = 'onnx-model-dir/decoder_model.onnx'

# Load the encoder model
encoder_session = InferenceSession(encoder_path)

input_ids = inputs['input_ids'].cpu().numpy()
attention_mask = inputs['attention_mask'].cpu().numpy()

# Create numpy arrays for the decoder inputs
decoder_inputs_ids = np.array([[60715]])

# Run the encoder model
encoder_inputs = {'input_ids': input_ids, 'attention_mask': attention_mask}

encoder_outputs = encoder_session.run(None, encoder_inputs)

# Load the decoder model
decoder_session = InferenceSession(decoder_path)

encoder_hidden_states = encoder_outputs[0]

# Prepare the decoder inputs
decoder_inputs = {
    'input_ids': decoder_inputs_ids,  # The starting token IDs for the decoder (should include <BOS> token)
    'encoder_attention_mask': attention_mask, 
    'encoder_hidden_states': encoder_hidden_states
}


# Run the decoder model
decoder_outputs = decoder_session.run(None, decoder_inputs)

for i, output in enumerate(decoder_outputs):
    print(f"Decoder output {i} shape: {output.shape}")


logits = torch.tensor(decoder_outputs[0])

# Take the argmax over the last dimension to get the predicted token IDs
predicted_token_ids = torch.argmax(logits, dim=-1).squeeze().cpu().numpy()

print(f"Predicted token IDs: {predicted_token_ids}")

# Convert the predicted token IDs into text, skipping special tokens
translated_text = tokenizer.decode(predicted_token_ids, skip_special_tokens=True)

print(f"Translated text: {translated_text}")

