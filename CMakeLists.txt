cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "~/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_CXX_STANDARD 17)

project(main)

# Path to ONNX Runtime
set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/external/Microsoft.ML.OnnxRuntime")

# Include directories for headers
include_directories(${ONNXRUNTIME_DIR}/build/native/include)

# Link directories for libraries
link_directories(${ONNXRUNTIME_DIR}/runtimes/win-x64/native)

add_executable(${PROJECT_NAME} main.cpp)

find_package(libzip CONFIG REQUIRED)
find_package(LibXml2 REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime libzip::zip LibXml2::LibXml2)


set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
)