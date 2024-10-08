from optimum.onnxruntime import ORTModelForSeq2SeqLM
from onnxruntime import InferenceSession, SessionOptions, get_available_providers
from transformers import AutoTokenizer, MarianTokenizer

text = '「一つは人間として生まれ変わり、新たな人生を歩むか。そしてもう一つは、天国的な所でおじい爺ちゃんみたいな暮らしをするか」'
tokenizer = MarianTokenizer.from_pretrained('Helsinki-NLP/opus-mt-ja-en')
inputs = tokenizer(text, return_tensors='pt')
modelPath = 'onnx-model-dir'
model = ORTModelForSeq2SeqLM.from_pretrained(modelPath)
generated = model.generate(**inputs)
print(tokenizer.decode(generated[0], skip_special_tokens=True))

