from transformers import AutoTokenizer
from optimum.onnxruntime import ORTModelForSeq2SeqLM
import multiprocessing as mp
import sys

# Global variable to store the model in each worker
model = None
onnx_model_path = 'onnx-model-dir'
mp.set_start_method("fork", force=True)

class encodedDataSingleton:
    _instance = None
    _encodedDataDict = None

    @staticmethod
    def get_instance():
        if encodedDataSingleton._instance is None:
            encodedDataSingleton()
        return encodedDataSingleton._instance

    def __init__(self):
        if encodedDataSingleton._instance is not None:
            raise Exception("This class is a singleton!")
        else:
            encodedDataSingleton._instance = self

    def set_encodedDataDict(self, data):
        print("Setting encoded data dict")
        sys.stdout.flush()
        self._encodedDataDict = data

    def get_encodedDataDict(self):
        return self._encodedDataDict
    
    def print_singletonDict(self):
        for values in self._encodedDataDict.values():
            print(values)
            sys.stdout.flush()

    def print_keys(self):
        print(self._encodedDataDict.keys())
        sys.stdout.flush()

# Initialize tokenizer once (can be shared across workers)
tokenizer = AutoTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')

# Function to initialize the model once per worker
def init_model():
    global model
    if model is None:
        print("Loading model in worker process.")
        sys.stdout.flush()
        model = ORTModelForSeq2SeqLM.from_pretrained(onnx_model_path)

# Tokenize text and return tensors
def tokenize_text(text):
    inputs = tokenizer(text, return_tensors="pt")
    print(f"\n\n\n\nOriginal Sentence: {text}")
    print(f"Tokenized Input: {inputs}")
    sys.stdout.flush()
    return inputs

# Detokenize text (accepts tokenized arrays)
def detokenize_text(text):
    print(f"Decoder output tokens: {text}")
    sys.stdout.flush()
    decodetext = tokenizer.decode(text, skip_special_tokens=True)
    return decodetext

def run_model_worker(encoded_data_queue, model_output_queue, worker_id):
    """Worker function to process translations."""
    print(f"Worker {worker_id} started.")
    sys.stdout.flush()

    init_model()  # Initialize model in worker

    while True:
        encoded_data = encoded_data_queue.get()

        if encoded_data is None:
            print(f"Worker {worker_id} received sentinel, stopping.")
            sys.stdout.flush()
            break

        encoded, chapterNum, position = encoded_data
        print(f"Worker {worker_id} is processing chapter {chapterNum} at position {position}.")
        sys.stdout.flush()

        try:
            print(f"Worker {worker_id} received input: {encoded}")
            sys.stdout.flush()
            # Model inference
            generated = model.generate(**encoded)
            print(f"Worker {worker_id} generated output: {generated}")
            sys.stdout.flush()

            # Put model output plus chapterNum and position in the output queue
            model_output_queue.put((generated[0], chapterNum, position))
        except Exception as e:
            print(f"Error in worker {worker_id} while processing chapter {chapterNum} at position {position}: {str(e)}")
            sys.stdout.flush()

    print(f"Worker {worker_id} exiting.")
    sys.stdout.flush()

    # End the worker process
    return



def run_model_multiprocessing(inputs, num_workers=4):
    """Function to distribute tasks among workers using multiprocessing.Process."""
    # Create queues for encoded data and model output
    encoded_data_queue = mp.Queue()
    model_output_queue = mp.Queue()

    # Add the inputs (dict) to the encoded data queue
    print(f"Adding {len(inputs)} inputs to the queue.")
    sys.stdout.flush()
    for (chapterNum, position), encoded_data in inputs.items():
        # Queue the data along with the chapter number and position
        print(f"Queueing input for chapter {chapterNum}, position {position}: {encoded_data}")
        sys.stdout.flush()
        encoded_data_queue.put((encoded_data, chapterNum, position))

    # Add a sentinel value for each worker to signal the end of the input data
    for _ in range(num_workers):
        encoded_data_queue.put(None)

    # Create and start worker processes
    processes = []
    for worker_id in range(num_workers):
        print(f"Starting worker {worker_id}.")
        sys.stdout.flush()
        process = mp.Process(target=run_model_worker, args=(encoded_data_queue, model_output_queue, worker_id))
        processes.append(process)
        process.start()

    # Wait for all worker processes to finish
    for process in processes:
        print(f"Waiting for process {process.pid} to terminate.")
        sys.stdout.flush()
        process.join()
        print(f"Process {process.pid} has terminated.")
        sys.stdout.flush()

    # Fetch the model outputs from the queue
    print("Fetching results from the output queue.")
    sys.stdout.flush()
    results = []
    while not model_output_queue.empty():
        try:
            output = model_output_queue.get(timeout=5)
            print(f"Received result from output queue: {output}")
            sys.stdout.flush()
            results.append(output)
        except mp.queues.Empty:
            print("Queue is empty.")
            sys.stdout.flush()

    if not results:
        print("No results were processed by the workers.")
        sys.stdout.flush()
    else:
        print(f"Processed {len(results)} results.")
        sys.stdout.flush()

    print("Main process exiting.")
    sys.stdout.flush()

    return results
