import torch
from transformers import MarianMTModel, MarianTokenizer
import onnx
from pathlib import Path

# Load the model and tokenizer
model_name = 'Helsinki-NLP/opus-mt-ja-en'
tokenizer = MarianTokenizer.from_pretrained(model_name)
model = MarianMTModel.from_pretrained(model_name)

# Set model to evaluation mode
model.eval()

# Sample text to trace the model
text = "私は猫です。"
inputs = tokenizer(text, return_tensors="pt", padding=True)

# ONNX export path
onnx_model_path = Path("marianmt_ja_en.onnx")

# Function to export the model
def export_onnx(model, inputs, onnx_model_path):
    # Define the model inputs and outputs
    input_names = ["input_ids", "attention_mask"]
    output_names = ["output"]

    # Export the model to ONNX format
    torch.onnx.export(
        model,                               # The model being exported
        (inputs['input_ids'], inputs['attention_mask']),  # Model inputs
        onnx_model_path.as_posix(),           # Where to save the ONNX file
        input_names=input_names,              # Input names for ONNX graph
        output_names=output_names,            # Output names for ONNX graph
        opset_version=12,                     # ONNX opset version
        dynamic_axes={                        # Dynamic axes for variable-length inputs
            "input_ids": {0: "batch_size", 1: "sequence_length"},
            "attention_mask": {0: "batch_size", 1: "sequence_length"},
            "output": {0: "batch_size", 1: "sequence_length"}
        }
    )

# Export the model
export_onnx(model, inputs, onnx_model_path)
print(f"Model exported to {onnx_model_path}")
