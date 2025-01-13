import fitz  # PyMuPDF
import os
from PIL import Image, ImageStat
from io import BytesIO
from transformers import MarianMTModel, MarianTokenizer
from fpdf import FPDF
import PyPDF2
from sudachipy import tokenizer
from sudachipy import dictionary

# Load the translation model and tokenizer
model_name = "Helsinki-NLP/opus-mt-ja-en"
tokenizer = MarianTokenizer.from_pretrained(model_name)
model = MarianMTModel.from_pretrained(model_name)

def translate_text(text):
    """Translates Japanese text to English."""
    inputs = tokenizer.encode(text, return_tensors="pt", truncation=True, max_length=512)
    translated = model.generate(inputs, max_length=512, num_beams=4, early_stopping=True)
    return tokenizer.decode(translated[0], skip_special_tokens=True)

def extract_text_from_pdf(file_path):
    """Extracts text from a PDF file."""
    with open(file_path, 'rb') as pdf_file:
        reader = PyPDF2.PdfReader(pdf_file)
        text = ""
        for page in reader.pages:
            text += page.extract_text() + "\n"
        return text

def split_japanese_text(text):
    """Splits Japanese text into sentences using SudachiPy."""
    tokenizer_obj = dictionary.Dictionary().create()
    mode = tokenizer.Tokenizer.SplitMode.C
    sentences = []
    current_sentence = ""
    for token in tokenizer_obj.tokenize(text, mode):
        current_sentence += token.surface()
        if token.surface() in "。！？":  # End of sentence markers
            sentences.append(current_sentence.strip())
            current_sentence = ""
    # Add any remaining text as a sentence
    if current_sentence:
        sentences.append(current_sentence.strip())
    return sentences

def filter_main_images(pdf_file, output_dir):
    """Extracts and filters main images from a PDF."""
    os.makedirs(output_dir, exist_ok=True)
    page_nums = len(pdf_file)
    main_images = []

    def is_main_image(image):
        # Convert to grayscale and analyze pixel intensity variance
        grayscale = image.convert("L")
        stat = ImageStat.Stat(grayscale)
        stddev = stat.stddev[0]  # Standard deviation of pixel values
        return stddev > 50  # Threshold for complexity

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

    # Make sure output_dir is empty
    for file in os.listdir(output_dir):
        os.remove(os.path.join(output_dir, file))

    # Open the PDF file
    pdf_file = fitz.open(input_pdf)

    # Step 1: Extract and filter main images
    print("Filtering main images...")
    main_images = filter_main_images(pdf_file, output_dir)

    # Step 2: Extract text from the PDF
    print("Extracting text from the PDF...")
    japanese_text = extract_text_from_pdf(input_pdf)

    # Step 3: Split the text into sentences
    print("Splitting text into sentences...")
    sentences = japanese_text.split("\u3002")  # Split on Japanese period character

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
