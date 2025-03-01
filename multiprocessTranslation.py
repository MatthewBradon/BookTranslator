import sys
import io
import transformers.generation as generation
sys.modules['transformers.generation_utils'] = generation
from transformers import AutoTokenizer
from optimum.onnxruntime import ORTModelForSeq2SeqLM
from readEncodedData import readEncodedData
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

def process_task(task, chapter_num_mode):
    """Process a single task and return translation results."""
    try:
        if chapter_num_mode == 0:
            encoded_data, chapterNum, position = task
        elif chapter_num_mode == 1:
            encoded_data, position = task

        # Perform model inference
        with torch.no_grad():
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

def run_model(file_path, chapter_num_mode=0):
    """Run model inference sequentially."""
    print("Reading encoded data...", flush=True)
    inputs = readEncodedData(file_path, chapter_num_mode)

    # Prepare tasks
    tasks = [(encoded_data, chapterNum, position) for (chapterNum, position), encoded_data in inputs.items()] if chapter_num_mode == 0 else \
            [(encoded_data, position) for position, encoded_data in inputs.items()]

    tasks = tasks[:100]  # Limit to 100 tasks for testing

    print(f"Processing {len(tasks)} tasks.", flush=True)
    
    results = []
    for task in tasks:
        result = process_task(task, chapter_num_mode)
        if result is not None:
            results.append(result)

    print(f"Processed {len(results)} results.", flush=True)
    return results

def main(file_path, chapter_num_mode=0):
    """Main function to handle file input/output."""
    print("Starting sequential processing.", flush=True)

    results = run_model(file_path, chapter_num_mode)

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
    if len(sys.argv) != 2:
        print("Usage: python single_process_translation.py <chapter_num_mode>")
        sys.exit(1)

    chapter_num_mode = int(sys.argv[1])

    # File path for input data
    file_path = "encodedTags.txt"

    # Run the main function
    sys.exit(main(file_path, chapter_num_mode))
