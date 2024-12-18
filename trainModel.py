import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader, Subset
from transformers import AutoTokenizer
import os
from lion_pytorch import Lion
from torch.cuda.amp import autocast, GradScaler
from sklearn.model_selection import KFold

# Define device
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

print("Device: ", device)
torch.cuda.empty_cache()
# Paths to data files
EN_FILE = "D:/JapaneseToEnglishDataset/CCMatrix/CCMatrix.en-ja.en"
JA_FILE = "D:/JapaneseToEnglishDataset/CCMatrix/CCMatrix.en-ja.ja"

# Define gradient accumulation steps
GRADIENT_ACCUMULATION_STEPS = 4
scaler = GradScaler()

# Step 1: Tokenization and Vocabulary
# Use Hugging Face tokenizer
tokenizer_src = AutoTokenizer.from_pretrained("facebook/nllb-200-distilled-600M")
tokenizer_tgt = AutoTokenizer.from_pretrained("facebook/nllb-200-distilled-600M")

en_vocab_size = tokenizer_src.vocab_size
ja_vocab_size = tokenizer_tgt.vocab_size

# Special tokens
PAD_IDX = tokenizer_src.pad_token_id
BOS_IDX = tokenizer_src.bos_token_id or tokenizer_src.cls_token_id
EOS_IDX = tokenizer_src.eos_token_id or tokenizer_src.sep_token_id

# Step 2: Dataset Definition
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

# Step 3: Define Transformer Model
class Transformer(nn.Module):
    def __init__(self, src_vocab_size, tgt_vocab_size, embed_size=512, num_heads=8, num_layers=6, ffn_hidden=2048):
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

# Step 4: Checkpointing
def save_checkpoint(model, optimizer, epoch, data_split, path="checkpoint.pth"):
    checkpoint = {
        "model_state_dict": model.state_dict(),
        "optimizer_state_dict": optimizer.state_dict(),
        "epoch": epoch,
        "data_split": data_split
    }
    torch.save(checkpoint, path)
    print(f"Checkpoint saved at epoch {epoch}")

def load_checkpoint(model, optimizer, path="checkpoint.pth"):
    if os.path.exists(path):
        checkpoint = torch.load(path)
        model.load_state_dict(checkpoint["model_state_dict"])
        optimizer.load_state_dict(checkpoint["optimizer_state_dict"])
        epoch = checkpoint["epoch"] + 1
        data_split = checkpoint["data_split"]
        print(f"Checkpoint loaded. Resuming from epoch {epoch}")
        return epoch, data_split
    print("No checkpoint found. Starting from scratch.")
    return 1, None

# Step 5: Epoch-wise Data Splitting
def split_dataset(dataset, num_splits, current_split=None):
    kf = KFold(n_splits=num_splits, shuffle=True, random_state=42)
    splits = list(kf.split(range(len(dataset))))
    if current_split is None:
        return splits
    train_indices, _ = splits[current_split]
    return Subset(dataset, train_indices)

# Step 6: Training Loop
def train_epoch(model, dataloader, optimizer, criterion, device):
    model.train()
    total_loss = 0
    optimizer.zero_grad()

    for batch_idx, (src, tgt) in enumerate(dataloader):
        src, tgt = src.to(device), tgt.to(device)
        tgt_input = tgt[:, :-1]
        tgt_output = tgt[:, 1:]

        with autocast():
            output = model(src, tgt_input)
            loss = criterion(output.view(-1, output.shape[-1]), tgt_output.contiguous().view(-1))
            loss = loss / GRADIENT_ACCUMULATION_STEPS

        scaler.scale(loss).backward()

        if (batch_idx + 1) % GRADIENT_ACCUMULATION_STEPS == 0 or (batch_idx + 1) == len(dataloader):
            scaler.step(optimizer)
            scaler.update()
            optimizer.zero_grad()

        total_loss += loss.item() * GRADIENT_ACCUMULATION_STEPS

    return total_loss / len(dataloader)

# Step 7: Training Setup
model = Transformer(en_vocab_size, ja_vocab_size).to(device)
optimizer = Lion(model.parameters(), lr=1e-4, weight_decay=1e-2)
criterion = nn.CrossEntropyLoss(ignore_index=PAD_IDX)

# Load checkpoint
EPOCHS = 10
splits = split_dataset(data, num_splits=EPOCHS)
start_epoch, data_split = load_checkpoint(model, optimizer)

for epoch in range(start_epoch, EPOCHS + 1):
    data_subset = split_dataset(data, num_splits=EPOCHS, current_split=epoch - 1)
    dataloader = DataLoader(data_subset, batch_size=8, shuffle=False, collate_fn=collate_fn)
    
    train_loss = train_epoch(model, dataloader, optimizer, criterion, device)
    print(f"Epoch {epoch}, Loss: {train_loss:.4f}")
    
    save_checkpoint(model, optimizer, epoch, splits[epoch - 1])

# Save the final model
torch.save(model.state_dict(), "transformer_translation.pt")
print("Final model saved!")
