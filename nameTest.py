import onnx

# Load the ONNX model
model = onnx.load('opus-mt-ja-en-ONNX\decoder_model.onnx')


for input in model.graph.input:
    print("Input name:", input.name)
    # print("Input shape:", input.type.tensor_type.shape)

# # Print the names of the output nodes
# for output in model.graph.output:
#     print("Output name:", output.name)