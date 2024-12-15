import torch
import re

def readEncodedData(file_path):
    """Read and parse encoded data from a file."""
    tag_dict = {}
    counter = 0
    with open(file_path, 'r') as file:
        for line in file:
            # Split the line into chapter, position, and tensor data
            parts = line.strip().split(',', 2)
            if len(parts) != 3:
                continue  # Skip lines that don't match the expected format

            chapter = int(parts[0])
            position = int(parts[1])

            # Pre-process the tensor data to remove 'tensor(...)'
            processed_tensor_data = re.sub(r"tensor\((\[.*?\])\)", r"\1", parts[2])

            # Convert the processed string into a Python dictionary
            tensor_data = eval(processed_tensor_data)  # Use eval after sanitizing

            tensor_data = {
                "input_ids": torch.tensor(tensor_data["input_ids"], dtype=torch.long),
                "attention_mask": torch.tensor(tensor_data["attention_mask"], dtype=torch.long),
            }


            # Use (chapter, position) as the key
            tag_dict[(chapter, position)] = tensor_data
            counter += 1

    return tag_dict