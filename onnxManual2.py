import torch
import numpy as np
from transformers import AutoTokenizer
from onnxruntime import InferenceSession



# Path to the directory containing the ONNX models and config files
model_path = 'onnx-model-dir'

# Load the tokenizer
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')

# Tokenize the input text
text = '「ねえカズマ、お金受け取/って来なさいよ！ もう、ギルド内の冒険者達のほとん殆どは、魔王の幹部討伐のしよう奨ほう報金貰ったわよ。もちろん私も！ でも見ての通り、もう結構飲んじゃったんだけどね！」'
inputs = tokenizer(text, return_tensors='pt')

# Load the ONNX models
encoder_path = 'onnx-model-dir/encoder_model.onnx'
decoder_path = 'onnx-model-dir/decoder_model.onnx'

# Load the encoder model
encoder_session = InferenceSession(encoder_path)

input_ids = inputs['input_ids'].cpu().numpy()
attention_mask = inputs['attention_mask'].cpu().numpy()

print("Input IDs: ", input_ids)

# Run the encoder model
encoder_inputs = {'input_ids': input_ids, 'attention_mask': attention_mask}
encoder_outputs = encoder_session.run(None, encoder_inputs)
encoder_hidden_states = encoder_outputs[0]

# Load the decoder model
decoder_session = InferenceSession(decoder_path)

# Autoregressive decoding with early stopping when no new information is added
max_length = 100  # Safety maximum sequence length
eos_token_id = 0
pad_token_id = 60715
generated_tokens = [pad_token_id]  # Start with <BOS> token
max_number_of_eos = 8
number_of_eos = 0

for step in range(max_length):
    decoder_input_ids = np.array([generated_tokens])

    # Prepare decoder inputs
    decoder_inputs = {
        'input_ids': decoder_input_ids,  # The decoder input so far
        'encoder_attention_mask': attention_mask,
        'encoder_hidden_states': encoder_hidden_states
    }
    # Run the decoder model
    decoder_outputs = decoder_session.run(None, decoder_inputs)
    logits = torch.tensor(decoder_outputs[0])

    # Take the argmax over the last dimension to get the predicted token ID
    next_token_id = torch.argmax(logits[:, -1, :], dim=-1).item()

    # Append the predicted token ID to the generated tokens
    generated_tokens.append(next_token_id)

    if next_token_id == eos_token_id:
        number_of_eos += 1
        if number_of_eos >= max_number_of_eos:
            break



print(f"Generated token IDs: {generated_tokens}")

# Convert the generated token IDs into text, skipping special tokens
translated_text = tokenizer.decode(generated_tokens, skip_special_tokens=True)

print(f"Translated text: {translated_text}")

