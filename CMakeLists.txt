cmake_minimum_required(VERSION 3.15)

project(BookTranslator)  # Rename the project to BookTranslator

# VCPKG config
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

set(CMAKE_CXX_STANDARD 17)


if(APPLE)
    set(APP_ICON "${CMAKE_SOURCE_DIR}/resources/BookTranslator.icns")
    set_source_files_properties(${APP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    add_executable(BookTranslator MACOSX_BUNDLE
        src/main.cpp
        src/GUI.cpp
        src/PDFTranslator.cpp
        src/EpubTranslator.cpp
        src/DocxTranslator.cpp
        src/HTMLTranslator.cpp
        ${APP_ICON}
    )

    set_target_properties(BookTranslator PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/Info.plist"
        OUTPUT_NAME "BookTranslator"
    )

else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
    add_executable(BookTranslator WIN32
        src/main.cpp 
        src/GUI.cpp
        src/PDFTranslator.cpp
        src/EpubTranslator.cpp
        src/DocxTranslator.cpp
        src/HTMLTranslator.cpp
    )

    set_property(TARGET BookTranslator PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_include_directories(BookTranslator PRIVATE src)


if(APPLE)
    add_custom_command(TARGET BookTranslator POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/translation"
            "$<TARGET_FILE_DIR:BookTranslator>/../Resources"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/translationConfig.json"
            "$<TARGET_FILE_DIR:BookTranslator>/../Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/rawEpub"
            "$<TARGET_FILE_DIR:BookTranslator>/../Resources/rawEpub/"

        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/onnx-model-dir"
            "$<TARGET_FILE_DIR:BookTranslator>/../Resources/onnx-model-dir"

        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/fonts"
            "$<TARGET_FILE_DIR:BookTranslator>/../Resources/fonts"

        COMMENT "Copying executables and directories into .app/Contents/"
    )
endif()


find_package(libzip CONFIG REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS process filesystem system)
find_package(Stb REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Cairo
pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(cairo-script-interpreter REQUIRED IMPORTED_TARGET cairo-script-interpreter)

# MuPDF
if(APPLE)
    set(MUPDF_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/mupdf/macBuild/include")
    set(MUPDF_LIB_DIR "${CMAKE_SOURCE_DIR}/external/mupdf/macBuild/lib")
    file(GLOB MUPDF_LIBS "${MUPDF_LIB_DIR}/*.a")
elseif(WIN32)
    set(MUPDF_LIB_DIR "${CMAKE_SOURCE_DIR}/external/mupdf/platform/win32/x64/Release")
    set(MUPDF_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/mupdf/include")
    file(GLOB MUPDF_LIBS "${MUPDF_LIB_DIR}/*.lib")
endif()

include_directories(${MUPDF_INCLUDE_DIR})

# Link libraries to the main BookTranslator target
if(APPLE)
    target_link_libraries(BookTranslator PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
        PkgConfig::cairo
        PkgConfig::cairo-script-interpreter
        Boost::process
        Boost::filesystem
        Boost::system
        nfd::nfd
        glad::glad
        glfw
        imgui::imgui
        libzip::zip
        LibXml2::LibXml2
        ${MUPDF_LIBS}
    )
elseif(WIN32)
    target_link_libraries(BookTranslator PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
        PkgConfig::cairo
        PkgConfig::cairo-script-interpreter
        Boost::process
        Boost::filesystem
        Boost::system
        nfd::nfd
        glad::glad
        glfw
        imgui::imgui
        libzip::zip
        LibXml2::LibXml2
        ${MUPDF_LIBS}
    )

    target_include_directories(BookTranslator PRIVATE
        ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows-static/include
    )
endif()

# Set output directory for the main executable (non-macOS usage)
# On macOS, the .app bundle logic overrides a typical EXE output.
set_target_properties(BookTranslator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}"
)


enable_testing()

add_executable(BookTranslatorTest
    tests/BookTranslatorTests.cpp
    src/EpubTranslator.cpp
    src/GUI.cpp
    src/PDFTranslator.cpp
    src/DocxTranslator.cpp
    src/HTMLTranslator.cpp
)

set_property(TARGET BookTranslatorTest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(BookTranslatorTest PRIVATE src)
target_include_directories(BookTranslatorTest PRIVATE ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows-static/include)
target_compile_definitions(BookTranslatorTest PRIVATE TESTING)

target_link_libraries(BookTranslatorTest PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    Catch2::Catch2
    Catch2::Catch2WithMain
    PkgConfig::cairo
    PkgConfig::cairo-script-interpreter
    Boost::process
    Boost::filesystem
    Boost::system
    nfd::nfd
    glad::glad
    glfw
    imgui::imgui
    libzip::zip
    LibXml2::LibXml2
    ${MUPDF_LIBS}
)

set_target_properties(BookTranslatorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
)

add_test(NAME BookTranslatorTests COMMAND BookTranslatorTest)