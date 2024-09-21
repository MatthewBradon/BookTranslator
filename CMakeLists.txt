cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "~/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_CXX_STANDARD 17)
set(PYTHONHOME "$ENV{PYTHON312_HOME}")

message(STATUS "PYTHONHOME: ${PYTHONHOME}")

project(main)

# Path to ONNX Runtime
set(ONNXRUNTIME_DIR ${CMAKE_SOURCE_DIR}/external/Microsoft.ML.OnnxRuntime.1.19.2)
# Include directories for headers
include_directories(${ONNXRUNTIME_DIR}/build/native/include)


# Check the architecture and set the appropriate runtime path
if(CMAKE_SYSTEM_NAME MATCHES "Darwin" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    message(STATUS "Targeting macOS ARM64 architecture")
    set(ONNX_RUNTIME_PATH ${ONNXRUNTIME_DIR}/runtimes/osx-arm64/native)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    message(STATUS "Targeting macOS x64 architecture")
    set(ONNX_RUNTIME_PATH ${ONNXRUNTIME_DIR}/runtimes/osx-x64/native)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
    message(STATUS "Targeting other ARM64 architecture")
    set(ONNX_RUNTIME_PATH ${ONNXRUNTIME_DIR}/runtimes/linux-arm64/native)
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

# If windows, set the paths to the Python3 executable, include directory, and library
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(Python3_EXECUTABLE "$ENV{PYTHON312_HOME}/python.exe")
    set(Python3_INCLUDE_DIR "$ENV{PYTHON312_HOME}/include")
    set(Python3_LIBRARY "$ENV{PYTHON312_HOME}/libs/python312.lib")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # Explicitly set the Python3 paths to the Homebrew Python
    set(Python3_EXECUTABLE "/opt/homebrew/bin/python3")
    set(Python3_INCLUDE_DIR "/opt/homebrew/Cellar/python@3.12/3.12.6/Frameworks/Python.framework/Versions/3.12/include/python3.12")
    set(Python3_LIBRARY "/opt/homebrew/Cellar/python@3.12/3.12.6/Frameworks/Python.framework/Versions/3.12/lib/libpython3.12.dylib")
endif()

# Manually set Python3 paths for CMake's find_package function
set(Python3_ROOT_DIR "$ENV{PYTHON312_HOME}")
set(Python3_FIND_STRATEGY LOCATION)

message(STATUS "Python3_ROOT_DIR: ${Python3_ROOT_DIR}")

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Print everything about the Python3 component
message(STATUS "Python3_FOUND: ${Python3_FOUND}")
message(STATUS "Python3_VERSION: ${Python3_VERSION}")
message(STATUS "Python3_EXECUTABLE: ${Python3_EXECUTABLE}")
message(STATUS "Python3_INCLUDE_DIR: ${Python3_INCLUDE_DIR}")
message(STATUS "Python3_LIBRARY: ${Python3_LIBRARY}")

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime libzip::zip LibXml2::LibXml2 Python3::Python pybind11::embed ${ONNX_RUNTIME_PATH}/libonnxruntime.dylib)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime libzip::zip LibXml2::LibXml2 Python3::Python pybind11::embed ${ONNXRUTIME_LIB}/onnxruntime.lib ${ONNXRUTIME_LIB}/onnxruntime_providers_shared.lib)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")

message(STATUS "Windows Copying DLLs to the output directory")

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
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")

message(STATUS "MacOS onnxruntime.dylib path: ${ONNX_RUNTIME_PATH}/libonnxruntime.dylib")

# Install Python packages and create virtual environment
add_custom_command(TARGET main POST_BUILD
    COMMAND ${Python3_EXECUTABLE} -m venv $<TARGET_FILE_DIR:main>/..
    COMMAND source $<TARGET_FILE_DIR:main>/../bin/activate
    COMMAND python -m pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt >> pip_install.log
    COMMAND python -m pip show transformers >> pip_show.log
)

# Copy dylibs to the output directory (important for runtime)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${ONNX_RUNTIME_PATH}/libonnxruntime.dylib
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
else()

endif()