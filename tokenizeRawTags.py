import sys
from transformers import AutoTokenizer

# Initialize tokenizer once
print("Loading tokenizer...")
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')

# Tokenize text and return tensors
def tokenize_text(text):
    inputs = tokenizer(text, return_tensors="pt")
    return inputs

def main(input_file, output_file):
    try:
        with open(input_file, "r", encoding="utf-8") as infile:
            lines = infile.readlines()

        print("Tokenizing the EPUB...")

        with open(output_file, "w", encoding="utf-8") as outfile:
            for line in lines:
                parts = line.strip().split(",")
                if len(parts) >= 4 and parts[0] == '0':  # Only process rows with tag type 0
                    chapter_num, position, text = parts[1], parts[2], parts[3]

                    encoded_text = tokenize_text(text)
                    input_ids_str = str(encoded_text["input_ids"]).replace("\n", " ").replace("\r", " ")
                    attention_mask_str = str(encoded_text["attention_mask"]).replace("\n", " ").replace("\r", " ")

                    outfile.write(f"{chapter_num},{position},{input_ids_str},{attention_mask_str}\n")
                    print(f"Processed chapter {chapter_num}, position {position}")
    except Exception as e:
        print(f"Error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py <input_file_path>")
        sys.exit(1)

    input_txt = sys.argv[1]
    output_txt = "encodedTags.txt"

    main(input_txt, output_txt)
