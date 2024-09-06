from transformers import MarianTokenizer

# Initialize the tokenizer
model_name = 'Helsinki-NLP/opus-mt-ja-en'
tokenizer = MarianTokenizer.from_pretrained(model_name)

def encode_text(text):
    """
    Encodes the input text into input_ids and attention_mask using the MarianMT tokenizer.

    Args:
        text (str): The input text to encode.

    Returns:
        input_ids (torch.Tensor): The tokenized input IDs.
        attention_mask (torch.Tensor): The attention mask for the input IDs.
    """
    inputs = tokenizer(text, return_tensors="pt", padding=True)
    input_ids = inputs['input_ids']
    attention_mask = inputs['attention_mask']
    return input_ids, attention_mask


def decodeText(token_ids):
    """
    Decodes the token IDs output by the ONNX model into human-readable text.

    Args:
        token_ids (torch.Tensor): The token IDs output from the model.

    Returns:
        str: The decoded text.
    """
    return tokenizer.decode(token_ids, skip_special_tokens=True)