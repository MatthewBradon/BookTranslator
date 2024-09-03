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
p_text = "私は猫です。"
inputs = tokenizer(p_text, return_tensors="pt", padding=True)
input_ids = inputs['input_ids'].to(device)
attention_mask = inputs['attention_mask'].to(device)

# Create dummy decoder input IDs
# Here we use the same length as input_ids, filled with zeros
# This is a placeholder; the actual values should match the expected input format
decoder_input_ids = torch.zeros_like(input_ids)

# Export the model to ONNX format
onnx_model_path = 'model.onnx'

# Set the model to evaluation mode
model.eval()

# Export the model
torch.onnx.export(
    model,
    (input_ids, attention_mask, decoder_input_ids),  # Sample inputs
    onnx_model_path,
    export_params=True,
    opset_version=11,  # Ensure this is compatible with your ONNX Runtime version
    input_names=['input_ids', 'attention_mask', 'decoder_input_ids'],
    output_names=['output'],
    dynamic_axes={
        'input_ids': {0: 'batch_size', 1: 'sequence_length'},
        'attention_mask': {0: 'batch_size', 1: 'sequence_length'},
        'decoder_input_ids': {0: 'batch_size', 1: 'sequence_length'},
        'output': {0: 'batch_size', 1: 'sequence_length'}
    }
)

print(f"Model exported to {onnx_model_path}")
