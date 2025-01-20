cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
project(main)

# Add the executable
add_executable(${PROJECT_NAME} main.cpp main.h GUI.cpp GUI.h PDFTranslator.cpp PDFTranslator.h EpubTranslator.cpp EpubTranslator.h Translator.h TranslatorFactory.h)

# Find the required packages
find_package(libzip CONFIG REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS process filesystem system)
find_package(Stb REQUIRED)
find_package(PkgConfig REQUIRED)

# Cairo configuration
pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(cairo-script-interpreter REQUIRED IMPORTED_TARGET cairo-script-interpreter)


# MuPDF configuration
set(MUPDF_LIB_DIR "${CMAKE_SOURCE_DIR}/external/mupdf/platform/win32/x64/Release")
set(MUPDF_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/mupdf/include")

# Include the MuPDF headers
include_directories(${MUPDF_INCLUDE_DIR})

include_directories("${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows/include")

# Dynamically collect all .lib files in the MuPDF library directory
file(GLOB MUPDF_LIBS "${MUPDF_LIB_DIR}/*.lib")

# Link libraries
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        Boost::process Boost::filesystem Boost::system 
        nfd::nfd glad::glad glfw imgui::imgui 
        libzip::zip LibXml2::LibXml2
    )
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        PkgConfig::cairo PkgConfig::cairo-script-interpreter
        Boost::process Boost::filesystem Boost::system 
        nfd::nfd glad::glad glfw imgui::imgui 
        libzip::zip LibXml2::LibXml2
        ${MUPDF_LIBS} # Include all MuPDF libraries
    )
endif()

# Set output directory for the executable
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
)
