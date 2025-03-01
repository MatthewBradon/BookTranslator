import sys
import io
import transformers.generation as generation
sys.modules['transformers.generation_utils'] = generation
from transformers import MarianTokenizer
from optimum.onnxruntime import ORTModelForSeq2SeqLM


# Force UTF-8 for stdout and stderr to prevent encoding issues
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")

onnx_model_path = 'onnx-model-dir'

# Initialize tokenizer once (can be shared across workers)
tokenizer = MarianTokenizer.from_pretrained('Helsinki-NLP/opus-mt-mul-en')
model = ORTModelForSeq2SeqLM.from_pretrained(onnx_model_path)




tasks = []
with open("rawTags.txt", "r", encoding="utf-8") as infile:
    lines = infile.readlines()
    for line in lines:
        parts = line.strip().split(",")
        if len(parts) >= 4 and parts[0] == '0':  # Only process rows with tag type 0
            chapter_num, position, text = parts[1], parts[2], parts[3]
            text = text.replace(">>jpn<<", "", 1).strip()

            encoded_text = tokenizer(text, return_tensors="pt")

            tasks.append((encoded_text, chapter_num, position))

# tasks = tasks[:50]

print(f"Processing {len(tasks)} tasks.", flush=True)

for task in tasks:
    try:
        encoded_data, chapterNum, position = task

        # Perform model inference
        generated = model.generate(
            **encoded_data,
        )

        # Detokenize the output
        text = tokenizer.decode(generated[0], skip_special_tokens=True)

        # Ensure UTF-8 safety
        print(f"Chapter {chapterNum}, Position {position}: {text}", flush=True)
    except Exception as e:
        print(f"Error processing task: {task}")
        print(f"Details: {e}")
        continue

