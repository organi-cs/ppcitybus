cmake_minimum_required(VERSION 3.20)
project(ppbus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(PPBUS_BUILD_TESTS "Build unit tests" ON)

# Several MinGW runtimes are commonly on PATH (Git, Strawberry, MSYS2); linking
# statically stops a mismatched libstdc++-6.dll from being picked up at runtime.
if(MINGW)
  add_link_options(-static)
endif()

function(ppbus_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    # MinGW has no sanitizers; libstdc++ assertions catch out-of-range indexing instead.
    target_compile_definitions(${target} PRIVATE $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
  endif()
endfunction()

add_library(ppbus_core STATIC
  src/core/config.cpp
  src/core/csv.cpp
  src/core/error.cpp
  src/core/geo.cpp
  src/core/network.cpp
  src/core/utf8.cpp
  src/core/validate.cpp
  src/core/version.cpp
)
target_include_directories(ppbus_core PUBLIC src)
target_compile_definitions(ppbus_core PRIVATE PPBUS_VERSION="${PROJECT_VERSION}")
ppbus_warnings(ppbus_core)

add_executable(ppbus src/cli/main.cpp)
target_link_libraries(ppbus PRIVATE ppbus_core)
ppbus_warnings(ppbus)

if(PPBUS_BUILD_TESTS)
  enable_testing()

  add_library(doctest INTERFACE)
  target_include_directories(doctest SYSTEM INTERFACE third_party/doctest)

  add_executable(ppbus_tests
    tests/main.cpp
    tests/test_config.cpp
    tests/test_csv.cpp
    tests/test_geo.cpp
    tests/test_network.cpp
    tests/test_utf8.cpp
    tests/test_validate.cpp
    tests/test_version.cpp
  )
  target_link_libraries(ppbus_tests PRIVATE ppbus_core doctest)
  ppbus_warnings(ppbus_tests)

  add_test(NAME unit COMMAND ppbus_tests)

  # The sample network must load and stay free of validation warnings.
  add_test(NAME sample_data_validates COMMAND ppbus validate --data ${CMAKE_SOURCE_DIR}/data/sample)
  set_tests_properties(sample_data_validates PROPERTIES FAIL_REGULAR_EXPRESSION "warning:")
endif()
