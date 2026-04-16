cmake_minimum_required(VERSION 3.20)
project(flux_t LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(flux_t INTERFACE)
target_include_directories(flux_t INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_executable(hello_flux
    examples/hello_flux.cpp
)
target_link_libraries(hello_flux PRIVATE flux_t)

add_subdirectory(tests)
