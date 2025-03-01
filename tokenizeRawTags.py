import sys
from transformers import AutoTokenizer

# Initialize tokenizer once
print("Loading tokenizer...")
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-mul-en')

# Tokenize text and return tensors
def tokenize_text(text):
    inputs = tokenizer(text, return_tensors="pt")
    return inputs

def main(input_file, output_file, chapter_num_mode):
    try:
        if chapter_num_mode == "0":
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
        elif chapter_num_mode == "1":
            with open(input_file, "r", encoding="utf-8") as infile:
                lines = infile.readlines()

            print("Tokenizing the EPUB...")

            with open(output_file, "w", encoding="utf-8") as outfile:
                for line in lines:
                    parts = line.strip().split(",")
                    if len(parts) >= 2:
                        position, text = parts[0], parts[1]

                        encoded_text = tokenize_text(text)
                        input_ids_str = str(encoded_text["input_ids"]).replace("\n", " ").replace("\r", " ")
                        attention_mask_str = str(encoded_text["attention_mask"]).replace("\n", " ").replace("\r", " ")

                        outfile.write(f"{position},{input_ids_str},{attention_mask_str}\n")
                        print(f"Processed position {position}")
        else:
            print("Invalid chapter_num_mode. Please enter 0 for EPUBs and 1 for PDFs.")
            sys.exit(1)           
        
        outfile.close()
    except Exception as e:
        print(f"Error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        # chapter num mode: EPUBs are split up intop chapters and positions but PDFs only have position
        print("Usage: python script.py <input_file_path> <chapter_num_mode>")
        sys.exit(1)

    input_txt = sys.argv[1]
    chapter_num_mode = sys.argv[2]
    output_txt = "encodedTags.txt"

    main(input_txt, output_txt, chapter_num_mode)
