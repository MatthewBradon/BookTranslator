import torch
from transformers import MarianMTModel, MarianTokenizer

# Define the model and tokenizer
model_name = 'Helsinki-NLP/opus-mt-ja-en'
tokenizer = MarianTokenizer.from_pretrained(model_name)
model = MarianMTModel.from_pretrained(model_name)
# Move the model to CPU (ONNX export usually happens on CPU)
device = torch.device("cpu")
model.to(device)

# Define example input text
p_text = "私は猫です。"  # Example Japanese text
inputs = tokenizer(p_text, return_tensors="pt", padding=True)
input_ids = inputs['input_ids'].to(device)
attention_mask = inputs['attention_mask'].to(device)

# Create dummy decoder input IDs using pad_token_id
decoder_input_ids = torch.full_like(input_ids, tokenizer.pad_token_id, dtype=torch.long)

# Ensure consistency of tensor types
print(f"Input IDs type: {input_ids.dtype}")
print(f"Attention Mask type: {attention_mask.dtype}")
print(f"Decoder Input IDs type: {decoder_input_ids.dtype}")

# Set the model to evaluation mode
model.eval()

# Export the encoder part of the model to ONNX format
onnx_encoder_path = 'marian_encoder.onnx'

# Export the encoder
torch.onnx.export(
    model.model.encoder, 
    (input_ids, attention_mask),  # Inputs for the encoder
    onnx_encoder_path,  # Path where ONNX model will be saved
    export_params=True,  # Store the trained weights
    opset_version=12,  # ONNX version
    do_constant_folding=True,  # Constant folding optimization
    input_names=['input_ids', 'attention_mask'],  # Model's input names
    output_names=['encoder_output'],  # Model's output names
    dynamic_axes={
        'input_ids': {0: 'batch_size', 1: 'sequence_length'}, 
        'attention_mask': {0: 'batch_size', 1: 'sequence_length'}, 
        'encoder_output': {0: 'batch_size', 1: 'sequence_length', 2: 'hidden_dim'}
    }
)

# Obtain encoder output
with torch.no_grad():
    encoder_output = model.model.encoder(input_ids, attention_mask)[0]

# Ensure encoder_output is float for compatibility
encoder_output = encoder_output.to(torch.float32)

# Export the decoder part of the model to ONNX format
onnx_decoder_path = 'marian_decoder.onnx'

# Ensure decoder_input_ids remain as integers (int64) as they are token IDs
decoder_input_ids = decoder_input_ids.to(torch.long)

# Export the decoder
torch.onnx.export(
    model.model.decoder,
    (decoder_input_ids, encoder_output, attention_mask),  # Inputs: decoder, encoder outputs, and attention mask
    onnx_decoder_path,  # Path where ONNX model will be saved
    export_params=True,
    opset_version=12,
    do_constant_folding=True,
    input_names=['decoder_input_ids', 'encoder_hidden_states', 'attention_mask'],
    output_names=['logits'],
    dynamic_axes={
        'decoder_input_ids': {0: 'batch_size', 1: 'sequence_length'},
        'encoder_hidden_states': {0: 'batch_size', 1: 'sequence_length', 2: 'hidden_dim'},
        'attention_mask': {0: 'batch_size', 1: 'sequence_length'},
        'logits': {0: 'batch_size', 1: 'sequence_length', 2: 'vocab_size'}
    }
)