from transformers import MarianTokenizer

# Load the tokenizer
tokenizer = MarianTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')

# Get the vocabulary
vocab = tokenizer.get_vocab()

# Check specific tokens
bos_token_id = 70000
eos_token_id =  70001

# Print token IDs
print(f'BOS Token ID: {bos_token_id}')
print(f'EOS Token ID: {eos_token_id}')

# Check if these tokens are unique in the vocabulary
print(f'BOS Token is in vocab: {bos_token_id in vocab.values()}')
print(f'EOS Token is in vocab: {eos_token_id in vocab.values()}')