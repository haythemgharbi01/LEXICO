cmake_minimum_required(VERSION 3.20)

project(Lexico LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(LEXICO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SourceCode/SourceFiles")
set(LEXICO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

find_package(BISON 3.0 REQUIRED)
find_package(FLEX 2.6 REQUIRED)
find_package(LLVM CONFIG REQUIRED)

file(MAKE_DIRECTORY "${LEXICO_GENERATED_DIR}")

# Keep generated Flex/Bison output in the build tree.  The checked-in generated
# files remain untouched, while a clean configure always uses the grammar and
# lexer currently in the source tree.
BISON_TARGET(
    LexicoParser
    "${LEXICO_SOURCE_DIR}/lexico.y"
    "${LEXICO_GENERATED_DIR}/lexico.tab.cpp"
    DEFINES_FILE "${LEXICO_GENERATED_DIR}/lexico.tab.hpp"
)

FLEX_TARGET(
    LexicoLexer
    "${LEXICO_SOURCE_DIR}/lexico.l"
    "${LEXICO_GENERATED_DIR}/lexico.lex.cpp"
)

ADD_FLEX_BISON_DEPENDENCY(LexicoLexer LexicoParser)

set(LEXICO_SOURCES
    "${LEXICO_SOURCE_DIR}/main.cpp"
    "${LEXICO_SOURCE_DIR}/driver.cpp"
    "${LEXICO_SOURCE_DIR}/ast.cpp"
    "${LEXICO_SOURCE_DIR}/symtab.cpp"
    "${LEXICO_SOURCE_DIR}/codegen.cpp"
    ${BISON_LexicoParser_OUTPUTS}
    ${FLEX_LexicoLexer_OUTPUTS}
)

add_executable(lexico ${LEXICO_SOURCES})

target_include_directories(lexico PRIVATE
    "${LEXICO_SOURCE_DIR}"
    "${LEXICO_GENERATED_DIR}"
    ${LLVM_INCLUDE_DIRS}
)

if(LLVM_DEFINITIONS)
    separate_arguments(LEXICO_LLVM_DEFINITIONS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
    target_compile_options(lexico PRIVATE ${LEXICO_LLVM_DEFINITIONS})
endif()

# Match the permissive GNU/Clang build used by the existing manual command.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lexico PRIVATE -Wall -Wextra -fpermissive)
elseif(MSVC)
    target_compile_options(lexico PRIVATE /W4)
endif()

find_library(LEXICO_LLVM_C_LIBRARY
    NAMES LLVM-C LLVM-C-20 LLVM-C-19 LLVM-C-18 LLVM-C-17 LLVM-C-16 LLVM-C-15
    HINTS ${LLVM_LIBRARY_DIRS}
)

if(NOT LEXICO_LLVM_C_LIBRARY)
    message(FATAL_ERROR
        "Could not find LLVM's C API library (LLVM-C). "
        "Set LLVM_DIR to the directory containing LLVMConfig.cmake or "
        "set LEXICO_LLVM_C_LIBRARY explicitly."
    )
endif()

target_link_libraries(lexico PRIVATE "${LEXICO_LLVM_C_LIBRARY}")

set_target_properties(lexico PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "Lexico source directory: ${LEXICO_SOURCE_DIR}")
message(STATUS "LLVM C API library: ${LEXICO_LLVM_C_LIBRARY}")