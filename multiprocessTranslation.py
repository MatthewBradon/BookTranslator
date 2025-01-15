from transformers import AutoTokenizer
from optimum.onnxruntime import ORTModelForSeq2SeqLM
import multiprocessing as mp
from readEncodedData import readEncodedData
import sys

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

def process_task(task):
    """Process a single task in a worker."""
    encoded_data, chapterNum, position = task

    print(f"Processing chapter {chapterNum} at position {position}.")
    sys.stdout.flush()

    init_model()  # Initialize model in worker

    try:
        # Perform model inference
        generated = model.generate(**encoded_data)
        # Detokenize
        text = tokenizer.decode(generated[0], skip_special_tokens=True)
        print(text)
        sys.stdout.flush()
        return chapterNum, position, text
    except Exception as e:
        print(f"Error while processing chapter {chapterNum} at position {position}: {str(e)}")
        sys.stdout.flush()
        return None

def run_model_multiprocessing(file_path, num_workers=2):
    """Function to distribute tasks among workers using multiprocessing.Pool."""
    # Read encoded data from the file
    inputs = readEncodedData(file_path)

    # Prepare tasks for workers
    tasks = [(encoded_data, chapterNum, position) for (chapterNum, position), encoded_data in inputs.items()]
    # tasks = tasks[:50]

    # Create a pool of worker processes
    with mp.Pool(processes=num_workers) as pool:
        # Submit tasks to the pool
        results = pool.map(process_task, tasks)

    # Filter out None results (if any errors occurred)
    results = [result for result in results if result is not None]

    if not results:
        print("No results were processed by the workers.")
        sys.stdout.flush()
    else:
        print(f"Processed {len(results)} results.")
        sys.stdout.flush()

    print("Main process exiting.")
    sys.stdout.flush()
    return results

def main(file_path):
    """Parent process that spawns workers."""
    print("Starting parent process.")
    sys.stdout.flush()

    # Run the multiprocessing task
    num_workers = 4
    results = run_model_multiprocessing(file_path, num_workers)

    # Write out results to translatedTags.txt
    with open("translatedTags.txt", "w", encoding="utf-8") as file:
        for result in results:
            file.write(f"{result[0]},{result[1]},{result[2]}\n")
    return 0

if __name__ == "__main__":
    # Ensure this block is only executed in the main process
    print("Hello from multiprocessTranslation.py", flush=True)

    # Required for PyInstaller to prevent infinite process spawning
    mp.freeze_support()

    mp.set_start_method("spawn", force=True)  # Set the spawn method
    file_path = "encodedTags.txt"
    sys.exit(main(file_path))
