cmake_minimum_required(VERSION 3.23)

set(UNITABLES_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

project(unitables
  DESCRIPTION "Lookup tables for Unicode properties"
  VERSION 0.1.0
  LANGUAGES C
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ============================================================================
# Unicode Character Database
# ============================================================================

set(UNITABLES_UNICODE_VERSION "17.0.0")
set(UNITABLES_UCD_URL
    "https://unicode.org/Public/${UNITABLES_UNICODE_VERSION}/ucd")
set(UNITABLES_UCD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ucd")

set(UNITABLES_UCD_INPUTS "")
set(UNITABLES_GEN_FLAGS "")

function(unitables_fetch_ucd flag uri sha256)
  cmake_path(GET uri FILENAME name)
  set(dest "${UNITABLES_UCD_DIR}/${name}")

  file(DOWNLOAD "${UNITABLES_UCD_URL}/${uri}" "${dest}"
       EXPECTED_HASH SHA256=${sha256} TLS_VERIFY ON STATUS status)

  list(GET status 0 code)
  if(NOT code EQUAL 0)
    list(GET status 1 message)
    message(FATAL_ERROR "unitables: download of ${uri} failed: ${message}")
  endif()

  list(APPEND UNITABLES_UCD_INPUTS "${dest}")
  list(APPEND UNITABLES_GEN_FLAGS "--${flag}=${dest}")
  set(UNITABLES_UCD_INPUTS "${UNITABLES_UCD_INPUTS}" PARENT_SCOPE)
  set(UNITABLES_GEN_FLAGS "${UNITABLES_GEN_FLAGS}" PARENT_SCOPE)
endfunction()

unitables_fetch_ucd(unicode-data
  "UnicodeData.txt"
  2e1efc1dcb59c575eedf5ccae60f95229f706ee6d031835247d843c11d96470c)

unitables_fetch_ucd(composition-exclusions
  "CompositionExclusions.txt"
  2f239196ef3b5b61db5cc476e9bd80f534d15aa1b74e1be1dea5d042a344c85f)

unitables_fetch_ucd(case-folding
  "CaseFolding.txt"
  ff8d8fefbf123574205085d6714c36149eb946d717a0c585c27f0f4ef58c4183)

unitables_fetch_ucd(grapheme-break-property
  "auxiliary/GraphemeBreakProperty.txt"
  d6b51d1d2ae5c33b451b7ed994b48f1f4dc62b2272a5831e7fd418514a6bae89)

unitables_fetch_ucd(emoji-data
  "emoji/emoji-data.txt"
  2cb2bb9455cda83e8481541ecf5b6dfda66a3bb89efa3fa7c5297eccf607b72b)

unitables_fetch_ucd(derived-core-properties
  "DerivedCoreProperties.txt"
  24c7fed1195c482faaefd5c1e7eb821c5ee1fb6de07ecdbaa64b56a99da22c08)

# ============================================================================
# Generate unitables_data.c
# ============================================================================

set(UNITABLES_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(UNITABLES_GENERATED_C "${UNITABLES_GEN_DIR}/unitables_data.c")

file(MAKE_DIRECTORY "${UNITABLES_GEN_DIR}")

add_custom_command(
  OUTPUT "${UNITABLES_GENERATED_C}"
  COMMAND Python3::Interpreter "${UNITABLES_DIR}/unitables.py"
          "${UNITABLES_GEN_DIR}" ${UNITABLES_GEN_FLAGS}
  DEPENDS "${UNITABLES_DIR}/unitables.py" ${UNITABLES_UCD_INPUTS}
  VERBATIM
)

# ============================================================================
# Library
# ============================================================================

add_library(unitables)
target_sources(unitables PRIVATE "${UNITABLES_DIR}/unitables.c")
target_sources(unitables PUBLIC
  FILE_SET HEADERS
  FILES 
    "${UNITABLES_DIR}/unitables.h"
    "${UNITABLES_GENERATED_C}"
)

target_compile_features(unitables PUBLIC c_std_99)
target_include_directories(unitables
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
  PRIVATE "${UNITABLES_GEN_DIR}"
)
