from transformers import AutoTokenizer
from optimum.onnxruntime import ORTModelForSeq2SeqLM
import multiprocessing as mp
from functools import partial
import sys
from readEncodedData import readEncodedData

# Global variable to store the model in each worker
model = None
onnx_model_path = 'onnx-model-dir'

# Initialize tokenizer once (can be shared across workers)
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')

# Function to initialize the model once per worker
def init_model():
    global model
    if model is None:
        print("Loading model in worker process.")
        sys.stdout.flush()
        model = ORTModelForSeq2SeqLM.from_pretrained(onnx_model_path)

# Worker initializer to set up global state
def worker_initializer(mode):
    global chapter_num_mode
    chapter_num_mode = mode  # Store mode globally for this worker

# Worker function
def process_task(task):
    global chapter_num_mode
    init_model()  # Initialize the model in the worker

    try:
        if chapter_num_mode == 0:
            encoded_data, chapterNum, position = task
        elif chapter_num_mode == 1:
            encoded_data, position = task

        # Perform model inference
        generated = model.generate(
            **encoded_data,
            no_repeat_ngram_size=3,   # Prevent repeating trigrams
            repetition_penalty=1.5,   # Penalize token repetition
        )

        # Detokenize the output
        text = tokenizer.decode(generated[0], skip_special_tokens=True)
        text = text.encode('utf-8', errors='replace').decode('utf-8')
        
        if chapter_num_mode == 0:
            print(f"Translated chapter {chapterNum} at position {position}: {text}", flush=True)
        elif chapter_num_mode == 1:
            print(f"Translated {position}: {text}", flush=True)
        

        if chapter_num_mode == 0:
            return chapterNum, position, text
        elif chapter_num_mode == 1:
            return position, text
    except Exception as e:
        error_msg = (
            f"Error while processing task {task}: {str(e)}"
            if chapter_num_mode == 1 else
            f"Error while processing chapter {task[1]} at position {task[2]}: {str(e)}"
        )
        print(error_msg, flush=True)
        return None

# Multiprocessing function
def run_model_multiprocessing(file_path, num_workers=4, chapter_num_mode=0):
    """Distribute tasks among workers using multiprocessing."""
    # Read encoded data
    inputs = readEncodedData(file_path, chapter_num_mode)

    # Prepare tasks based on the mode
    tasks = [(encoded_data, chapterNum, position) for (chapterNum, position), encoded_data in inputs.items()] if chapter_num_mode == 0 else \
            [(encoded_data, position) for position, encoded_data in inputs.items()]

    print(f"Processing {len(tasks)} tasks.", flush=True)

    # Set up the pool with an initializer to pass chapter_num_mode to each worker
    with mp.Pool(processes=num_workers, initializer=worker_initializer, initargs=(chapter_num_mode,)) as pool:
        results = pool.map(process_task, tasks)

    # Filter out None results
    results = [result for result in results if result is not None]

    print(f"Processed {len(results)} results.", flush=True)
    return results

# Main function
def main(file_path, chapter_num_mode=0):
    """Parent process to coordinate workers."""
    print("Starting parent process.", flush=True)

    # Run the multiprocessing task
    num_workers = 4
    results = run_model_multiprocessing(file_path, num_workers, chapter_num_mode)

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

# Entry point for the script
if __name__ == "__main__":
    mp.freeze_support()  # Required for Windows and PyInstaller
    print("Hello from multiprocessTranslation.py", flush=True)

    # Ensure proper usage
    if len(sys.argv) != 2:
        print("Usage: python multiprocessTranslation.py <chapter_num_mode>")
        sys.exit(1)

    chapter_num_mode = int(sys.argv[1])

    # Set the multiprocessing start method to spawn
    mp.set_start_method("spawn", force=True)

    # File path for input data
    file_path = "encodedTags.txt"

    # Run the main function
    sys.exit(main(file_path, chapter_num_mode))
