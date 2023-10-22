cmake_minimum_required(VERSION 3.24)
project(docstring_generator_ext VERSION 1.0 LANGUAGES CXX)

set(pybind11_DIR "../venv/lib/python3.12/site-packages/pybind11/share/cmake/pybind11")
set(PYBIND11_PYTHON_VERSION 3.12)

find_package(Python 3.12 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(${PROJECT_NAME} docstringFormat.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE pybind11::embed)

pybind11_extension(${PROJECT_NAME})
if(NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
    # Strip unnecessary sections of the binary on Linux/macOS
    pybind11_strip(${PROJECT_NAME})
endif()

if ("$ENV{GCC}")
    target_compile_options(${PROJECT_NAME} PRIVATE
            -Wall
            -Wextra
            -Werror
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wfloat-conversion
            -fno-omit-frame-pointer
            -fsanitize=address
            )
endif ()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        CMAKE_CXX_FLAGS O3
        CMAKE_CXX_FLAGS_RELEASE O3
        CXX_VISIBILITY_PRESET "hidden")