# Epub Translator Prototype in C++
This is a prototype for my final year project


Only thing of note is that I got onnxruntime using nuget. I link it in the Cmake.

The other libxml2 and libzip I got from vcpkg

### PyBind: Embedding Python into C++
I am embedding python into the C++ so we can use the transformers library since there is no C++ version for the Marian Tokenizer. While there is MarianNMT (which is written in C++) to my knowledge and reading there is no tokenizer in C++.

In the CMAKE you can see that I am my local python installation rather than the python interpreter that gets with vcpkg that is because when importing `transformers` it has a dependency on `_socket` and the python from vcpkg is missing some of the core standard libraries such as `_socket`

If you want to build this project set an environment variable called `PYTHON312_HOME` and set it to your local Python 3.12 installation make sure that you have the development packages (The headers used to link C and Python its inside the include directory). You can install the development packages by clicking the box in the python installer


