import torch
import re

def readEncodedData(file_path):
    """Read and parse encoded data from a file."""
    tag_dict = {}

    # Regex pattern to match the entire tensor blocks
    tensor_pattern = re.compile(r"tensor\(\[\[(.*?)\]\]\),tensor\(\[\[(.*?)\]\]\)")

    with open(file_path, 'r') as file:
        for line in file:
            # Match the chapter, position, and tensor blocks
            parts = line.strip().split(',', 2)  # First split on the first two commas
            if len(parts) != 3:
                print(f"Skipping malformed line: {line}")
                continue

            try:
                chapter = int(parts[0])
                position = int(parts[1])

                # Extract `input_ids` and `attention_mask` using regex
                match = tensor_pattern.search(parts[2])
                if not match:
                    print(f"Skipping line due to tensor mismatch: {line}")
                    continue

                # Parse `input_ids`
                input_ids_numbers = list(map(int, match.group(1).split(',')))
                input_ids = torch.tensor([input_ids_numbers], dtype=torch.long)

                # Parse `attention_mask`
                attention_mask_numbers = list(map(int, match.group(2).split(',')))
                attention_mask = torch.tensor([attention_mask_numbers], dtype=torch.long)

                # Check and log shape mismatches
                if input_ids.size(1) != attention_mask.size(1):
                    print(f"Shape mismatch at Chapter {chapter}, Position {position}:")
                    print(f"input_ids: {input_ids.size()}, attention_mask: {attention_mask.size()}")

                # Store the tensors in a dictionary
                tensor_data = {
                    "input_ids": input_ids,
                    "attention_mask": attention_mask,
                }

                # Use (chapter, position) as the key
                tag_dict[(chapter, position)] = tensor_data

            except Exception as e:
                print(f"Error processing line: {line}")
                print(f"Details: {e}")
                continue

    return tag_dict

if __name__ == "__main__":
    file_path = "encodedTags.txt"
    inputs = readEncodedData(file_path)
    
    # # Print out input data for debugging
    for (chapter, position), tensor_data in inputs.items():
        print(f"Chapter {chapter}, Position {position}:")
        print(f"input_ids: {tensor_data['input_ids']}")
        print(f"attention_mask: {tensor_data['attention_mask']}")
        print()
        
        