cmake_minimum_required(VERSION 3.17)

set(CMAKE_CXX_STANDARD 17)

project(hilok)

find_package(Python COMPONENTS Interpreter Development.Module)

############# conan
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})

# see: https://github.com/conan-io/cmake-conan
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake"
                TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_configure(REQUIRES catch2/3.2.0 pybind11/2.10.1 GENERATORS cmake_find_package)
conan_cmake_autodetect(settings)
conan_cmake_install(PATH_OR_REFERENCE . BUILD missing SETTINGS ${settings})


############# catch2
option(ENABLE_TESTS "Enable tests" OFF)
option(ENABLE_TSAN "Enable tsan" OFF)
option(ENABLE_COVERAGE "Enable coverage" OFF)
option(ENABLE_TRACE "Enable lock tracing" OFF)

if (${ENABLE_TESTS})

find_package(Catch2 3 REQUIRED)
find_package(Threads REQUIRED)
add_executable(tests tests/test.cpp src/hilok.cpp src/recsh.cpp src/hilok.hpp src/recsh.hpp)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(tests PRIVATE Threads::Threads)
target_include_directories(tests PRIVATE src)

include(CTest)
include(Catch)
catch_discover_tests(tests)

enable_testing()


############# tsan
if (${ENABLE_TSAN})
  target_compile_options(tests PRIVATE "-fsanitize=thread")

  # Enables dynamic_annotations.h to actually generate code
  target_compile_definitions(tests  PRIVATE "-DDYNAMIC_ANNOTATIONS_ENABLED")

  # changes atomicops to use the tsan implementations
  target_compile_definitions(tests  PRIVATE "-DTHREAD_SANITIZER")

  target_compile_definitions(tests  PRIVATE "-D_GLIBCXX_EXTERN_TEMPLATE=0")

  target_link_libraries(tests PRIVATE tsan)
endif()

if (${ENABLE_TRACE})
    target_compile_definitions(tests  PRIVATE "-DHILOK_TRACE")
endif()

############# codecov
if (${ENABLE_COVERAGE})
    SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -fno-inline -fno-inline-small-functions -fno-default-inline")
    SET(CMAKE_C_FLAGS_DEBUG "-O0 -fno-inline -fno-inline-small-functions -fno-default-inline")
    set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/externals/CMake-codecov/cmake" ${CMAKE_MODULE_PATH})
    find_package(codecov)
    add_coverage(tests)
    list(APPEND LCOV_REMOVE_PATTERNS "'/usr/*'")
    coverage_evaluate()
endif()

endif()

############# extra warnings & lint
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-ignored-optimization-argument)
endif()


############# pybind11
find_package(pybind11 REQUIRED)

pybind11_add_module(hilok MODULE src/pybind.cpp src/hilok.cpp src/recsh.cpp)

target_link_libraries(hilok PRIVATE pybind11::headers)
if (NOT ${ENABLE_TESTS})
# settings taken from pybind11 docs
# see: https://pybind11.readthedocs.io/en/stable/cmake/index.html
set_target_properties(hilok PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON VISIBILITY_INLINES_HIDDEN ON)
endif()

############# clang-tidy
if (${ENABLE_TESTS})
if (NOT APPLE)
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" "-checks=-*,modernize-*,-*-use-trailing-return-type")
set_target_properties(hilok PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
set_target_properties(tests PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
endif()
endif()

