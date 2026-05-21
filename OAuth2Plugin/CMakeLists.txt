cmake_minimum_required(VERSION 3.10)
project(OAuth2Plugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Dependencies
find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# Plugin Core Library
add_library(${PROJECT_NAME} OBJECT)

# Include Directories
target_include_directories(${PROJECT_NAME}
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/storage
        ${CMAKE_CURRENT_SOURCE_DIR}/src/models
        ${CMAKE_CURRENT_SOURCE_DIR}/src/services
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
        ${CMAKE_CURRENT_SOURCE_DIR}/include/oauth2/models
)

# Sources
file(GLOB_RECURSE PLUGIN_SOURCES 
    "src/*.cc"
)

target_sources(${PROJECT_NAME} PRIVATE ${PLUGIN_SOURCES})

# Links
target_link_libraries(${PROJECT_NAME} PUBLIC Drogon::Drogon OpenSSL::Crypto)

# Compiler Specific Options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/include/oauth2/orm_compat.h")
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    # Silence C++17 codecvt deprecation warning for Drogon ORM generated code
    target_compile_definitions(${PROJECT_NAME} PRIVATE _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/include/oauth2/orm_compat.h")
endif()

# Views (if any core views exist, but currently they are in Server)

# Install Rules
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    EXPORT OAuth2PluginTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/oauth2 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT OAuth2PluginTargets
    FILE OAuth2PluginConfig.cmake
    NAMESPACE OAuth2Plugin::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OAuth2Plugin
)
