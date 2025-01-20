import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader, Subset
from transformers import AutoTokenizer
from lion_pytorch import Lion
from torch.cuda.amp import autocast, GradScaler
import os
import numpy as np
import time

# Define device
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
print("Device: ", device)

# Clear cache
torch.cuda.empty_cache()

# Paths to data files
EN_FILE = "CCMatrix.en-ja.en"
JA_FILE = "CCMatrix.en-ja.ja"

# Gradient accumulation steps
GRADIENT_ACCUMULATION_STEPS = 8
scaler = GradScaler()

# Tokenization and Vocabulary
tokenizer_src = AutoTokenizer.from_pretrained("facebook/nllb-200-distilled-600M")
tokenizer_tgt = AutoTokenizer.from_pretrained("facebook/nllb-200-distilled-600M")

en_vocab_size = tokenizer_src.vocab_size
ja_vocab_size = tokenizer_tgt.vocab_size

PAD_IDX = tokenizer_src.pad_token_id
BOS_IDX = tokenizer_src.bos_token_id or tokenizer_src.cls_token_id
EOS_IDX = tokenizer_src.eos_token_id or tokenizer_src.sep_token_id

# Dataset Definition
class TranslationDataset(Dataset):
    def __init__(self, src_file, tgt_file, src_tokenizer, tgt_tokenizer):
        with open(src_file, encoding="utf-8") as f:
            self.src_data = f.readlines()
        with open(tgt_file, encoding="utf-8") as f:
            self.tgt_data = f.readlines()
        self.src_tokenizer = src_tokenizer
        self.tgt_tokenizer = tgt_tokenizer

    def __len__(self):
        return len(self.src_data)

    def __getitem__(self, idx):
        src = self.src_tokenizer(self.src_data[idx].strip(), return_tensors='pt', padding=True, truncation=True).input_ids[0]
        tgt = self.tgt_tokenizer(self.tgt_data[idx].strip(), return_tensors='pt', padding=True, truncation=True).input_ids[0]
        return torch.tensor([BOS_IDX] + src.tolist() + [EOS_IDX]), torch.tensor([BOS_IDX] + tgt.tolist() + [EOS_IDX])

def collate_fn(batch):
    src_batch, tgt_batch = [], []
    for src, tgt in batch:
        src_batch.append(src)
        tgt_batch.append(tgt)
    src_batch = nn.utils.rnn.pad_sequence(src_batch, batch_first=True, padding_value=PAD_IDX)
    tgt_batch = nn.utils.rnn.pad_sequence(tgt_batch, batch_first=True, padding_value=PAD_IDX)
    return src_batch, tgt_batch

data = TranslationDataset(EN_FILE, JA_FILE, tokenizer_src, tokenizer_tgt)

# Transformer Model
class Transformer(nn.Module):
    def __init__(self, src_vocab_size, tgt_vocab_size, embed_size=256, num_heads=4, num_layers=4, ffn_hidden=512):
        super().__init__()
        self.src_embed = nn.Embedding(src_vocab_size, embed_size, padding_idx=PAD_IDX)
        self.tgt_embed = nn.Embedding(tgt_vocab_size, embed_size, padding_idx=PAD_IDX)
        self.pos_encoder = nn.Transformer(
            d_model=embed_size,
            nhead=num_heads,
            num_encoder_layers=num_layers,
            num_decoder_layers=num_layers,
            dim_feedforward=ffn_hidden,
            batch_first=True
        )
        self.fc_out = nn.Linear(embed_size, tgt_vocab_size)

    def forward(self, src, tgt):
        src = self.src_embed(src)
        tgt = self.tgt_embed(tgt)
        output = self.pos_encoder(src, tgt)
        output = self.fc_out(output)
        return output

# Checkpointing
def save_checkpoint(model, optimizer, epoch, current_split, path="checkpoint.pth"):
    checkpoint = {
        "model_state_dict": model.state_dict(),
        "optimizer_state_dict": optimizer.state_dict(),
        "epoch": epoch,
        "current_split": current_split
    }
    torch.save(checkpoint, path)
    print(f"Checkpoint saved at epoch {epoch}, split {current_split}")

def load_checkpoint(model, optimizer, path="checkpoint.pth"):
    if os.path.exists(path):
        checkpoint = torch.load(path)
        model.load_state_dict(checkpoint["model_state_dict"])
        optimizer.load_state_dict(checkpoint["optimizer_state_dict"])
        epoch = checkpoint["epoch"] + 1
        current_split = checkpoint["current_split"]
        print(f"Checkpoint loaded. Resuming from epoch {epoch}, split {current_split}")
        return epoch, current_split
    print("No checkpoint found. Starting from scratch.")
    return 1, 0

# Non-overlapping Subset Splitting
def split_dataset_nonoverlapping(dataset, num_splits, current_split, seed=42):
    total_size = len(dataset)
    indices = np.arange(total_size)
    rng = np.random.default_rng(seed)
    rng.shuffle(indices)
    subset_size = total_size // num_splits
    start_idx = current_split * subset_size
    end_idx = total_size if current_split == num_splits - 1 else start_idx + subset_size
    subset_indices = indices[start_idx:end_idx]
    return Subset(dataset, subset_indices)

# Training Loop
def train_epoch(model, dataloader, optimizer, criterion, device):
    model.train()
    total_loss = 0
    optimizer.zero_grad()
    start_time = time.time()

    for batch_idx, (src, tgt) in enumerate(dataloader):
        src, tgt = src.to(device), tgt.to(device)
        tgt_input = tgt[:, :-1]
        tgt_output = tgt[:, 1:]

        with autocast(device.type):
            output = model(src, tgt_input)
            loss = criterion(output.view(-1, output.shape[-1]), tgt_output.contiguous().view(-1))
            loss = loss / GRADIENT_ACCUMULATION_STEPS

        scaler.scale(loss).backward()

        if (batch_idx + 1) % GRADIENT_ACCUMULATION_STEPS == 0 or (batch_idx + 1) == len(dataloader):
            scaler.step(optimizer)
            scaler.update()
            optimizer.zero_grad()

        total_loss += loss.item() * GRADIENT_ACCUMULATION_STEPS

        if (batch_idx + 1) % 10 == 0:
            progress = (batch_idx + 1) / len(dataloader) * 100
            elapsed_time = time.time() - start_time
            print(f"Progress: {progress:.2f}% (Batch {batch_idx + 1}/{len(dataloader)}), Elapsed Time: {elapsed_time:.2f}s")

    return total_loss / len(dataloader)

# Training Setup
EPOCHS = 10
SUBSET_SPLITS = 10  # Number of splits per epoch
BATCH_SIZE = 4

model = Transformer(en_vocab_size, ja_vocab_size).to(device)
optimizer = Lion(model.parameters(), lr=1e-4, weight_decay=1e-2)
criterion = nn.CrossEntropyLoss(ignore_index=PAD_IDX)

start_epoch, start_split = load_checkpoint(model, optimizer)

for epoch in range(start_epoch, EPOCHS + 1):
    for split_idx in range(start_split, SUBSET_SPLITS):
        data_subset = split_dataset_nonoverlapping(data, num_splits=SUBSET_SPLITS, current_split=split_idx)
        dataloader = DataLoader(data_subset, batch_size=BATCH_SIZE, shuffle=True, collate_fn=collate_fn)
        
        print(f"Epoch {epoch}, Split {split_idx + 1}/{SUBSET_SPLITS}: Subset contains {len(data_subset)} samples.")
        train_loss = train_epoch(model, dataloader, optimizer, criterion, device)
        print(f"Epoch {epoch}, Split {split_idx + 1}, Loss: {train_loss:.4f}")

        save_checkpoint(model, optimizer, epoch, split_idx)

    start_split = 0  # Reset split index for the next epoch

torch.save(model.state_dict(), "transformer_translation.pt")
print("Final model saved!")
