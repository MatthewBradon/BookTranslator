import fitz  # PyMuPDF
import os

# File path you want to render images from
pdf_file_path = "C:/Users/matth/Desktop/Kono Subarashii Sekai ni Shukufuku wo! [JP]/Konosuba Volume 1 [JP].pdf"

# Directory to save the rendered images
output_dir = "RenderedImages"
os.makedirs(output_dir, exist_ok=True)

# Open the PDF file
pdf_file = fitz.open(pdf_file_path)

# Get the number of pages
page_nums = len(pdf_file)

# Render each page as an image
for page_num in range(page_nums):
    page = pdf_file[page_num]
    
    # Render the page at 300 DPI for high quality
    pix = page.get_pixmap(dpi=300)
    
    # Save the rendered image as PNG
    output_path = os.path.join(output_dir, f"page_{page_num + 1}.png")
    pix.save(output_path)
    print(f"Saved rendered page {page_num + 1} to {output_path}")

print(f"Rendering complete. {page_nums} pages saved to {output_dir}")
