cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "/Users/xsmoked/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_CXX_STANDARD 17)
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
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# Set the RPATH to tell the executable where to find dynamic libraries
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)




# Print everything about the Python3 component
message(STATUS "Python3_FOUND: ${Python3_FOUND}")
message(STATUS "Python3_VERSION: ${Python3_VERSION}")
message(STATUS "Python3_EXECUTABLE: ${Python3_EXECUTABLE}")
message(STATUS "Python3_INCLUDE_DIR: ${Python3_INCLUDE_DIR}")
message(STATUS "Python3_LIBRARY: ${Python3_LIBRARY}")

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_link_libraries(${PROJECT_NAME} PRIVATE libzip::zip LibXml2::LibXml2 imgui::imgui Python3::Python pybind11::embed ${ONNX_RUNTIME_PATH}/libonnxruntime.1.19.2.dylib)
    set(CMAKE_INSTALL_RPATH "${ONNX_RUNTIME_PATH}")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime glfw3 libzip::zip LibXml2::LibXml2 imgui::imgui OpenGL::GL Python3::Python pybind11::embed ${ONNXRUTIME_LIB}/onnxruntime.lib ${ONNXRUTIME_LIB}/onnxruntime_providers_shared.lib)
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

message(STATUS "MacOS onnxruntime.dylib path: ${ONNX_RUNTIME_PATH}/")

# Install Python packages and create virtual environment
add_custom_command(TARGET main POST_BUILD
    COMMAND ${Python3_EXECUTABLE} -m venv $<TARGET_FILE_DIR:main>/..
    COMMAND source $<TARGET_FILE_DIR:main>/../bin/activate
    COMMAND python -m pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt >> pip_install.log
    COMMAND python -m pip show transformers >> pip_show.log
)

# # Copy dylibs to the output directory (important for runtime)
# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy
#     ${ONNX_RUNTIME_PATH}/libonnxruntime.1.19.2.dylib
#     $<TARGET_FILE_DIR:${PROJECT_NAME}>
#     # Remame the dylib to onnxruntime.dylib
#     # COMMAND mv $<TARGET_FILE_DIR:${PROJECT_NAME}>/libonnxruntime.dylib $<TARGET_FILE_DIR:${PROJECT_NAME}>/libonnxruntime.1.19.dylib
# )


else()

endif()