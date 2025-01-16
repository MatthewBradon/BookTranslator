cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
project(main)

# Add the executable
add_executable(${PROJECT_NAME} main.cpp main.h GUI.cpp GUI.h PDFParser.cpp PDFParser.h)

# Find the required packages
find_package(libzip CONFIG REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS process filesystem system)
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER_CPP REQUIRED IMPORTED_TARGET poppler-cpp)



target_include_directories(${PROJECT_NAME} PRIVATE
    ${libzip_INCLUDE_DIRS}
    ${imgui_INCLUDE_DIRS}
)

# Link libraries
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::POPPLER_CPP Boost::process Boost::filesystem Boost::system nfd::nfd glad::glad glfw imgui::imgui libzip::zip LibXml2::LibXml2)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::POPPLER_CPP Boost::process Boost::filesystem Boost::system nfd::nfd glad::glad glfw imgui::imgui libzip::zip LibXml2::LibXml2)
endif()

# Set output directory for the executable
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
)
