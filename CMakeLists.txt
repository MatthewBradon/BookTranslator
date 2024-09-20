cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "~/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_CXX_STANDARD 17)
set(PYTHONHOME "$ENV{PYTHON312_HOME}")

project(main)

# Path to ONNX Runtime
set(ONNXRUNTIME_DIR ${CMAKE_SOURCE_DIR}/external/Microsoft.ML.OnnxRuntime.1.19.2)
# Include directories for headers
include_directories(${ONNXRUNTIME_DIR}/build/native/include)


# Check the architecture and set the appropriate runtime path
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
    message(STATUS "Targeting ARM64 architecture")
    set(ONNX_RUNTIME_PATH ${ONNXRUNTIME_DIR}/runtimes/win-arm64/native)
else()
    message(STATUS "Targeting x64 architecture")
    set(ONNX_RUNTIME_PATH ${ONNXRUNTIME_DIR}/runtimes/win-x64/native)
endif()

# Link directories for libraries
link_directories(${ONNX_RUNTIME_PATH})
set(ONNXRUNTIME_LIB ${ONNX_RUNTIME_PATH})

add_executable(${PROJECT_NAME} main.cpp)

find_package(libzip CONFIG REQUIRED)
find_package(LibXml2 REQUIRED)

# Manually set Python3 paths to ensure they are found
set(Python3_EXECUTABLE "$ENV{PYTHON312_HOME}/python.exe")
set(Python3_INCLUDE_DIR "$ENV{PYTHON312_HOME}/include")
set(Python3_LIBRARY "$ENV{PYTHON312_HOME}/libs/python312.lib")

# Manually set Python3 paths for CMake's find_package function
set(Python3_ROOT_DIR "$ENV{PYTHON312_HOME}")
set(Python3_FIND_STRATEGY LOCATION)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Print everything about the Python3 component
message(STATUS "Python3_FOUND: ${Python3_FOUND}")
message(STATUS "Python3_VERSION: ${Python3_VERSION}")
message(STATUS "Python3_EXECUTABLE: ${Python3_EXECUTABLE}")
message(STATUS "Python3_INCLUDE_DIR: ${Python3_INCLUDE_DIR}")
message(STATUS "Python3_LIBRARY: ${Python3_LIBRARY}")

target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime libzip::zip LibXml2::LibXml2 Python3::Python pybind11::embed ${ONNXRUTIME_LIB}/onnxruntime.lib ${ONNXRUTIME_LIB}/onnxruntime_providers_shared.lib)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
)

# Install Python packages and create virtual environment
add_custom_command(TARGET main POST_BUILD
    COMMAND ${Python3_EXECUTABLE} -m venv $<TARGET_FILE_DIR:main>/..
    COMMAND $<TARGET_FILE_DIR:main>/../Scripts/activate.bat
    COMMAND python -m pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt >> pip_install.log
    COMMAND python -m pip show transformers >> pip_show.log
)

# Copy DLLs to the output directory (important for runtime)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${ONNX_RUNTIME_PATH}/onnxruntime.dll
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${ONNX_RUNTIME_PATH}/onnxruntime_providers_shared.dll
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)