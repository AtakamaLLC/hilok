cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 17)

project(hilok)

find_package(Python COMPONENTS Interpreter Development)

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
find_package(Catch2 3 REQUIRED)
add_executable(tests tests/test.cpp src/hilok.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_include_directories(tests PRIVATE src)
include(CTest)
include(Catch)
catch_discover_tests(tests)
enable_testing()

############# pybind11

find_package(pybind11 REQUIRED)

pybind11_add_module(hilok src/pybind.cpp src/hilok.cpp)

target_link_libraries(hilok PRIVATE pybind11::headers)

# settings taken from pybind11 docs
# see: https://pybind11.readthedocs.io/en/stable/cmake/index.html
set_target_properties(hilok PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON CXX_VISIBILITY_PRESET ON VISIBILITY_INLINES_HIDDEN ON)
