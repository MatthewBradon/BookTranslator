import sys
import io
import transformers.generation as generation
sys.modules['transformers.generation_utils'] = generation
from transformers import AutoTokenizer
from optimum.onnxruntime import ORTModelForSeq2SeqLM
import torch

# Force UTF-8 for stdout and stderr to prevent encoding issues
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")

onnx_model_path = 'onnx-model-dir'

# Load model and tokenizer once
print("Loading model...", flush=True)
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-mul-en')
model = ORTModelForSeq2SeqLM.from_pretrained(onnx_model_path)
print("Model loaded successfully.", flush=True)


def create_tasks(input_file_path="rawTags.txt", chapter_num_mode=0):
    tasks = []
    try:
        if chapter_num_mode == 0:
            with open(input_file_path, "r", encoding="utf-8") as infile:
                lines = infile.readlines()
      
            for line in lines:
                parts = line.strip().split(",")
                if len(parts) >= 4 and parts[0] == '0':  # Only process rows with tag type 0
                    chapter_num, position, text = parts[1], parts[2], parts[3]

                    tasks.append((chapter_num, position, text))
            
        elif chapter_num_mode == 1:
            with open(input_file_path, "r", encoding="utf-8") as infile:
                lines = infile.readlines()

            for line in lines:
                parts = line.strip().split(",")
                if len(parts) >= 2:
                    position, text = parts[0], parts[1]

                    tasks.append((position, text))
        else:
            print("Invalid chapter_num_mode. Please enter 0 for EPUBs and 1 for PDFs.")
            sys.exit(1)           
        
        return tasks
    except Exception as e:
        print(f"Error occurred: {e}")

def process_task(task, chapter_num_mode):
    """Process a single task and return translation results."""
    try:
        if chapter_num_mode == 0:
            chapterNum, position, text = task
        elif chapter_num_mode == 1:
            position, text = task

        # Perform model inference
        with torch.no_grad():

            encoded_data = tokenizer(text, return_tensors="pt")
            generated = model.generate(**encoded_data)
            text = tokenizer.decode(generated[0], skip_special_tokens=True)

        # Ensure UTF-8 safety
        text = text.encode('utf-8', errors='replace').decode('utf-8')

        if chapter_num_mode == 0:
            print(f"Translated chapter {chapterNum} at position {position}: {text}", flush=True)
            return chapterNum, position, text
        elif chapter_num_mode == 1:
            print(f"Translated {position}: {text}", flush=True)
            return position, text
    except Exception as e:
        print(f"Error processing task: {task}, Details: {e}", flush=True)
        return None

def run_model(input_file_path="rawTags.txt", chapter_num_mode=0):
    """Run model inference sequentially."""

    tasks = create_tasks(input_file_path, chapter_num_mode)

    tasks = tasks[:100]  # Limit to 100 tasks for testing

    print(f"Processing {len(tasks)} tasks.", flush=True)
    
    results = []
    for task in tasks:
        result = process_task(task, chapter_num_mode)
        if result is not None:
            results.append(result)

    print(f"Processed {len(results)} results.", flush=True)
    return results

def main(input_file_path="rawTags.txt",chapter_num_mode=0):
    """Main function to handle file input/output."""
    print("Starting sequential processing.", flush=True)

    results = run_model(input_file_path, chapter_num_mode)

    # Write results to file
    output_file = "translatedTags.txt"
    with open(output_file, "w", encoding="utf-8") as file:
        for result in results:
            if chapter_num_mode == 0 and len(result) == 3:
                chapterNum, position, text = result
                file.write(f"{chapterNum},{position},{text}\n")
            elif chapter_num_mode == 1 and len(result) == 2:
                position, text = result
                file.write(f"{position},{text}\n")

    print(f"Results written to {output_file}.", flush=True)
    return 0

if __name__ == "__main__":
    print("Hello from single-process translation script.", flush=True)

    # Ensure proper usage
    if len(sys.argv) != 3:
        print("Usage: python single_process_translation.py <input_file_path> <chapter_num_mode>")
        sys.exit(1)

    input_file_path = str(sys.argv[1])
    chapter_num_mode = int(sys.argv[2])

    print(f"Input file path: {input_file_path}", flush=True)
    print(f"Chapter num mode: {chapter_num_mode}", flush=True)

    # Run the main function
    sys.exit(main(input_file_path,chapter_num_mode))
