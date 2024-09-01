import zipfile
import os

# Create a new EPUB file
with zipfile.ZipFile('output.epub', 'w', zipfile.ZIP_DEFLATED) as zf:
    for root, _, files in os.walk('export/'):
        for file in files:
            full_path = os.path.join(root, file)
            relative_path = os.path.relpath(full_path, 'export/')
            zf.write(full_path, relative_path)
