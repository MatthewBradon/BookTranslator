import fitz  # PyMuPDF
import os
from PIL import Image, ImageStat
from io import BytesIO
from transformers import MarianMTModel, MarianTokenizer
from fpdf import FPDF
import PyPDF2
from sudachipy import dictionary, tokenizer as sudachi_tokenizer
import re

# Load the translation model and tokenizer
model_name = "Helsinki-NLP/opus-mt-ja-en"
tokenizer = MarianTokenizer.from_pretrained(model_name)
model = MarianMTModel.from_pretrained(model_name)

def translate_text(text):
    """Translates Japanese text to English."""
    inputs = tokenizer.encode(text, return_tensors="pt", truncation=True, max_length=512)
    translated = model.generate(inputs, max_length=512, num_beams=4, early_stopping=True)
    return tokenizer.decode(translated[0], skip_special_tokens=True)

def split_long_sentences(sentence, max_length=300):
    """Splits long sentences into smaller ones based on logical breakpoints."""
    # Logical breakpoints: commas, conjunctions, etc.
    breakpoints = ["、", "。", "しかし", "そして", "だから", "そのため"]
    sentences = []
    current_chunk = ""
    token_count = 0

    for token in sentence:
        current_chunk += token
        token_count += 1

        # Check if the current token is a logical breakpoint
        if token in breakpoints:
            if len(current_chunk) > max_length:  # Only split if chunk exceeds max_length
                sentences.append(current_chunk.strip())
                current_chunk = ""
                token_count = 0

        # Force a split if the token count exceeds the max_length
        if token_count > max_length:
            sentences.append(current_chunk.strip())
            current_chunk = ""
            token_count = 0

    # Add any remaining text as the last sentence
    if current_chunk:
        sentences.append(current_chunk.strip())

    return sentences



def split_japanese_text(text):
    """Intelligently splits Japanese text into sentences with advanced handling for long sentences."""
    # Normalize repeating quotes
    text = re.sub(r"「{2,}", "「", text)
    text = re.sub(r"」{2,}", "」", text)


    tokenizer_obj = dictionary.Dictionary().create()
    mode = sudachi_tokenizer.Tokenizer.SplitMode.C
    sentences = []
    current_sentence = ""

    # Flag to track if we are inside a quote
    in_quote = False

    for token in tokenizer_obj.tokenize(text, mode):
        current_sentence += token.surface()

        # Handle opening quotes and closing quotes
        if token.surface() == "「":
            in_quote = True
        elif token.surface() == "」":
            in_quote = False

        # Check for sentence-ending punctuation
        if token.surface() in "。！？" and not in_quote:  # Avoid splitting inside quotes
            if len(current_sentence) > 300:  # Handle long sentences
                sentences.extend(split_long_sentences(current_sentence.strip(), max_length=300))
            else:
                sentences.append(current_sentence.strip())
            current_sentence = ""

        # Handle cases where sentence ends inside quotes
        if token.surface() == "。」" and not in_quote:
            if len(current_sentence) > 300:  # Handle long sentences
                sentences.extend(split_long_sentences(current_sentence.strip(), max_length=300))
            else:
                sentences.append(current_sentence.strip())
            current_sentence = ""

    # Add any remaining text as the last sentence
    if current_sentence:
        if len(current_sentence) > 300:
            sentences.extend(split_long_sentences(current_sentence.strip(), max_length=300))
        else:
            sentences.append(current_sentence.strip())

    return sentences


def extract_text_from_pdf(file_path):
    """Extracts and intelligently tokenizes text from a PDF."""
    tokenizer_obj = dictionary.Dictionary().create()
    mode = sudachi_tokenizer.Tokenizer.SplitMode.C
    sentences = []

    with open(file_path, 'rb') as pdf_file:
        reader = PyPDF2.PdfReader(pdf_file)
        for page in reader.pages:
            text = page.extract_text()
            # Split text intelligently
            page_sentences = split_japanese_text(text)
            sentences.extend(page_sentences)

    return sentences


def is_main_image(image):
    # Convert to grayscale and analyze pixel intensity variance
    grayscale = image.convert("L")
    stat = ImageStat.Stat(grayscale)
    stddev = stat.stddev[0]  # Standard deviation of pixel values
    return stddev > 50  # Threshold for complexity

def filter_main_images(pdf_file, output_dir):
    """Extracts and filters main images from a PDF."""
    os.makedirs(output_dir, exist_ok=True)
    page_nums = len(pdf_file)
    main_images = []

    for page_num in range(page_nums):
        page = pdf_file[page_num]
        pix = page.get_pixmap(dpi=300)  # Render the page as an image
        image = Image.open(BytesIO(pix.tobytes("png")))

        if is_main_image(image):
            output_path = os.path.join(output_dir, f"main_image_page_{page_num + 1}.png")
            image.save(output_path, "PNG")
            main_images.append(output_path)
            print(f"Saved main image from page {page_num + 1} to {output_path}")
        else:
            print(f"Skipped non-main image on page {page_num + 1}")

    return main_images

def create_translated_pdf_with_images(main_images, translated_sentences, output_path):
    """Creates a new PDF with main images at the beginning followed by translated text."""
    pdf = FPDF()
    pdf.set_auto_page_break(auto=True, margin=5)

    # Add main images
    for image_path in main_images:
        pdf.add_page()
        pdf.image(image_path, x=10, y=10, w=190)

    # Add translated text
    pdf.add_page()
    pdf.add_font("DejaVu", style="", fname="DejaVuSans.ttf", uni=True)
    pdf.set_font("DejaVu", size=12)

    for sentence in translated_sentences:
        pdf.multi_cell(0, 10, sentence)
        pdf.ln()

    pdf.output(output_path)

def main():
    input_pdf = "C:/Users/matth/Desktop/Kono Subarashii Sekai ni Shukufuku wo! [JP]/Konosuba Volume 1 [JP].pdf"
    output_dir = "FilteredImages"
    output_pdf = "translated_with_images.pdf"

    # If output_pdf already exists, delete it
    if os.path.exists(output_pdf):
        os.remove(output_pdf)
        

    # Make sure output_dir is empty
    for file in os.listdir(output_dir):
        os.remove(os.path.join(output_dir, file))

    # Open the PDF file
    pdf_file = fitz.open(input_pdf)

    # Step 1: Extract and filter main images
    print("Filtering main images...")
    main_images = filter_main_images(pdf_file, output_dir)

    # Step 2: Extract text from the PDF
    print("Extracting and tokenizing text from the PDF...")
    sentences = extract_text_from_pdf(input_pdf)



    # Step 4: Translate each sentence
    print("Translating sentences...")
    translated_sentences = []
    for sentence in sentences:
        sentence = sentence.strip()
        if sentence:  # Skip empty sentences
            sentence = sentence.replace("\n", "").replace(" ", "")
            print(f"Translating: {sentence}")
            translated_sentence = translate_text(sentence)
            print(f"Translated: {translated_sentence}")
            translated_sentences.append(translated_sentence)

    # Step 5: Create the translated PDF with images
    print("Creating the translated PDF with images...")
    create_translated_pdf_with_images(main_images, translated_sentences, output_pdf)
    print(f"Translated PDF saved to {output_pdf}")

if __name__ == "__main__":
    main()
