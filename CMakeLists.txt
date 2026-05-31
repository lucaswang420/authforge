cmake_minimum_required(VERSION 3.10)

project(oauth2-plugin-example
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "OAuth2 / OIDC Plugin & Reference Server (Drogon)")

# Load shared CMake modules (after project() so toolchain is fully applied)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Version)
include(Compatibility)
include(Sanitizers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_subdirectory(OAuth2Plugin)
add_subdirectory(OAuth2Server)
