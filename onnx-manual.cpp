#include <iostream>
#include <zip.h>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <string>
#include <regex>
#include <vector>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <libxml/xmlstring.h>
#include <onnxruntime_cxx_api.h>
#include <iostream>
#include <Python.h>
#include <random>
#include <pybind11/embed.h>
#include <pybind11/numpy.h>  
#include <mutex>
#include <condition_variable>
#include <thread>
#include <time.h>

#define EOS_TOKEN_ID 0
#define PAD_TOKEN_ID 60715


const char* encoder_path = "onnx-model-dir/encoder_model.onnx";
const char* decoder_path = "onnx-model-dir/decoder_model.onnx";

std::vector<int64_t> processNumpyArray(pybind11::array_t<int64_t> inputArray) {
    try{

        // Get the buffer information
        pybind11::buffer_info buf = inputArray.request();

        // Check the number of dimensions
        // if (buf.ndim != 2) {
        //     throw std::runtime_error("Number of dimensions must be one");
        // }

        // Get the pointer to the data
        int64_t* ptr = static_cast<int64_t*>(buf.ptr);

        std::vector<int64_t> vec(ptr, ptr + buf.size);
        // for(const auto& value : vec) {
        //     std::cout << value << std::endl;
        // }
        return vec;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return {};
    }

}

std::string run_onnx_translation(std::string input_text, pybind11::module& tokenizer_module) {
    
    pybind11::object tokenize_text = tokenizer_module.attr("tokenize_text");
    pybind11::object detokenize_text = tokenizer_module.attr("detokenize_text");

    // Call Python tokenize_text function
    pybind11::tuple tokenized = tokenize_text(input_text);
    std::vector<int64_t> input_ids = processNumpyArray(tokenized[0].cast<pybind11::array_t<int64_t>>());
    std::vector<int64_t> attention_mask = processNumpyArray(tokenized[1].cast<pybind11::array_t<int64_t>>());


    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "test");
    Ort::SessionOptions session_options;

    Ort::Session encoder_session(env, encoder_path, session_options);
    Ort::Session decoder_session(env, decoder_path, session_options);




    // Prepare input for the encoder
    std::vector<int64_t> input_shape = {1, static_cast<int64_t>(input_ids.size())};
    std::vector<int64_t> attention_shape = {1, static_cast<int64_t>(attention_mask.size())};

    std::vector<const char*> input_node_names = {"input_ids", "attention_mask"};
    std::vector<const char*> output_node_names = {"last_hidden_state"};

    // Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    Ort::Value input_tensor = Ort::Value::CreateTensor<int64_t>(memory_info, input_ids.data(), input_ids.size(), input_shape.data(), input_shape.size());
    Ort::Value attention_tensor = Ort::Value::CreateTensor<int64_t>(memory_info, attention_mask.data(), attention_mask.size(), attention_shape.data(), attention_shape.size());

    // Run encoder model
    std::vector<Ort::Value> encoder_inputs;
    encoder_inputs.push_back(std::move(input_tensor));    // Move the tensor instead of copying
    encoder_inputs.push_back(std::move(attention_tensor)); // Move the attention tensor


    auto encoder_output_tensors = encoder_session.Run(
        Ort::RunOptions{nullptr}, input_node_names.data(), 
        encoder_inputs.data(), input_node_names.size(), 
        output_node_names.data(), 1);

    // Get encoder output tensor
    float* encoder_output_data = encoder_output_tensors[0].GetTensorMutableData<float>();

    // Get encoder output shape
    Ort::TensorTypeAndShapeInfo encoder_output_info = encoder_output_tensors[0].GetTensorTypeAndShapeInfo();
    size_t encoder_output_size = encoder_output_info.GetElementCount();
    std::vector<int64_t> encoder_output_shape = encoder_output_info.GetShape();

    std::cout << std::endl;

    std::vector<float> encoder_output(encoder_output_data, encoder_output_data + encoder_output_size);

    // DECODER


    std::vector<const char*> decoder_input_node_names = {"encoder_attention_mask", "input_ids", "encoder_hidden_states"};
    std::vector<const char*> decoder_output_node_names = {"logits"};

    // Set up the decoder input tensor
    std::vector<int64_t> token_ids = {PAD_TOKEN_ID};
    std::vector<int64_t> decoder_input_shape = {1, static_cast<int64_t>(token_ids.size())};

    Ort::Value decoder_input_tensor = Ort::Value::CreateTensor<int64_t>(
        memory_info, token_ids.data(), token_ids.size(),
        decoder_input_shape.data(), decoder_input_shape.size());


    Ort::Value decoder_attention_tensor = Ort::Value::CreateTensor<int64_t>(
        memory_info, attention_mask.data(), attention_mask.size(),
        attention_shape.data(), attention_shape.size());

    // Encoder hidden states (output from the encoder model)
    Ort::Value encoder_hidden_states_tensor = Ort::Value::CreateTensor<float>(
        memory_info, encoder_output.data(), encoder_output.size(),
        encoder_output_shape.data(), encoder_output_shape.size());


    // Prepare decoder inputs
    std::vector<Ort::Value> decoder_inputs;


    decoder_inputs.push_back(std::move(decoder_attention_tensor));  // attention mask
    decoder_inputs.push_back(std::move(decoder_input_tensor));  // token ids
    decoder_inputs.push_back(std::move(encoder_hidden_states_tensor));  // encoder hidden states


    size_t max_decode_steps = 60;  // Avoid infinite 
    
    int eos_counter = 0;
    int max_eos_counter = 5;

    for (size_t step = 0; step < max_decode_steps; ++step) {
        // Run the decoder model
        auto decoder_output_tensors = decoder_session.Run(
            Ort::RunOptions{nullptr}, decoder_input_node_names.data(),
            decoder_inputs.data(), decoder_input_node_names.size(),
            decoder_output_node_names.data(), 1);

        // Extract logits from the output tensor
        auto logits_tensor = std::move(decoder_output_tensors[0]);
        auto logits_shape = logits_tensor.GetTensorTypeAndShapeInfo().GetShape();

        // Assuming the last dimension of the logits is the vocabulary size
        int64_t seq_length = logits_shape[1]; // sequence length (step count so far)
        int64_t vocab_size = logits_shape[2]; // vocabulary size

        // Extract logits corresponding to the last token in the sequence (logits[:, -1, :])
        std::vector<float> logits(logits_tensor.GetTensorMutableData<float>(), 
                                logits_tensor.GetTensorMutableData<float>() + std::accumulate(logits_shape.begin(), logits_shape.end(), 1, std::multiplies<int64_t>()));
        
        // Get the logits of the last token
        std::vector<float> last_token_logits(logits.end() - vocab_size, logits.end());

        // Get the predicted token ID (argmax over the last dimension)
        int next_token_id = std::distance(last_token_logits.begin(), std::max_element(last_token_logits.begin(), last_token_logits.end()));

        // Check if the predicted token is EOS
        if (next_token_id == EOS_TOKEN_ID) {
            ++eos_counter;
            if (eos_counter >= max_eos_counter) {
                break;  // Stop decoding if we've seen enough EOS tokens
            }
        }

        // Update the decoder input tensor with the new token_ids
        token_ids.push_back(next_token_id);
        decoder_input_shape = {1, static_cast<int64_t>(token_ids.size())};  // Update shape to reflect new sequence length

        decoder_input_tensor = Ort::Value::CreateTensor<int64_t>(
            memory_info, token_ids.data(), token_ids.size(),
            decoder_input_shape.data(), decoder_input_shape.size());

        // Replace the old decoder input tensor in decoder_inputs with the new tensor
        decoder_inputs[1] = std::move(decoder_input_tensor);  // Move the new token_ids tensor


    }


    // Detokenize the output tokens to text
    pybind11::array_t<int64_t> token_ids_array(token_ids.size(), token_ids.data());
    pybind11::object detokenized_text = detokenize_text(token_ids_array);
    std::string output_text = detokenized_text.cast<std::string>();

    // std::cout << "Output text: " << output_text << std::endl;

    return output_text;
}

void processChapter(const std::filesystem::path& chapterPath, pybind11::module& EncodeDecode) {
    std::ifstream file(chapterPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << chapterPath << std::endl;
        return;
    }

    // Read the entire content of the chapter file
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Parse the content using libxml2
    htmlDocPtr doc = htmlReadMemory(data.c_str(), data.size(), NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) {
        std::cerr << "Failed to parse HTML content." << std::endl;
        return;
    }

    // Create an XPath context
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (!xpathCtx) {
        std::cerr << "Failed to create XPath context." << std::endl;
        xmlFreeDoc(doc);
        return;
    }

    // Extract all <p> and <img> tags
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//p | //img"), xpathCtx);
    if (!xpathObj) {
        std::cerr << "Failed to evaluate XPath expression." << std::endl;
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return;
    }

    xmlNodeSetPtr nodes = xpathObj->nodesetval;
    int nodeCount = (nodes) ? nodes->nodeNr : 0;

    std::string chapterContent;
    for (int i = 0; i < nodeCount; ++i) {
        xmlNodePtr node = nodes->nodeTab[i];
        if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("p")) == 0) {
            // Extract text content of <p> tag
            xmlChar* textContent = xmlNodeGetContent(node);
            if (textContent) {
                try{

                    std::string pText(reinterpret_cast<char*>(textContent));


                    if (!pText.empty()) {
                        // Create tokens using encodeText function from Python
                        
                        // std::string strippedText = stripHtmlTags(pText);

                        // if the first character of strippedText is a space, remove it
                        // if (strippedText[0] == ' ') {
                        //     strippedText = strippedText.substr(1);
                        // }

                        // std::string translatedText = runPyBindTranslate(strippedText, EncodeDecode);

                        // std::cout << "Translation text: " << translatedText << std::endl;
                        // chapterContent += "<p>" + translatedText + "</p>\n";
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error: " << e.what() << std::endl;
                }
                
                
                xmlFree(textContent);
            }
        } else if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("img")) == 0) {
            // Extract src attribute of <img> tag
            xmlChar* src = xmlGetProp(node, reinterpret_cast<const xmlChar*>("src"));
            if (src) {
                std::string imgSrc(reinterpret_cast<char*>(src));
                std::string imgFilename = std::filesystem::path(imgSrc).filename().string();
                chapterContent += "<img src=\"../Images/" + imgFilename + "\"/>\n";
                xmlFree(src);
            }
        }
    }

    // If chapterContent is empty, preserve the original body content
    if (chapterContent.empty()) {
        xmlChar* bodyContent = xmlNodeGetContent(xmlDocGetRootElement(doc));
        if (bodyContent) {
            chapterContent = reinterpret_cast<char*>(bodyContent);
            xmlFree(bodyContent);
        }
    }

    // Construct the XHTML content
    std::string xhtmlContent =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<!DOCTYPE html>\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\">\n"
        "<head>\n"
        "  <title>" + chapterPath.filename().string() + "</title>\n"
        "</head>\n"
        "<body>\n" + chapterContent +
        "</body>\n"
        "</html>";

    // Write the processed content to a new file
    std::filesystem::path outputPath = "export/OEBPS/Text/" + chapterPath.filename().string();
    std::ofstream outFile(outputPath);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open output file: " << outputPath << std::endl;
        xmlXPathFreeObject(xpathObj);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return;
    }

    outFile << xhtmlContent;
    outFile.close();

    std::cout << "Chapter done: " << chapterPath.filename().string() << std::endl;

    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
}

