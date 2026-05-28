cmake_minimum_required(VERSION 3.16)
project(OAuth2Plugin VERSION ${OAUTH2_PROJECT_VERSION} LANGUAGES CXX)

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
        # ORM-generated model .cc files emit quote-includes of sibling
        # model headers (e.g. `#include "Oauth2Clients.h"` from
        # `Oauth2AccessTokens.cc`). Per spec §0 the generator output is
        # exempt from rewrite, so we keep this PRIVATE include path
        # rather than mutating the .cc files. (design §7.4 mentioned
        # removing this entry; the actual implementation must keep it
        # because ORM generator quote-include style is non-negotiable.)
        ${CMAKE_CURRENT_SOURCE_DIR}/include/oauth2/models
)

# Sources
file(GLOB_RECURSE PLUGIN_SOURCES 
    "src/*.cc"
)

target_sources(${PROJECT_NAME} PRIVATE ${PLUGIN_SOURCES})

# Links
target_link_libraries(${PROJECT_NAME} PUBLIC Drogon::Drogon OpenSSL::Crypto)

# Compiler compatibility (shared module)
oauth2_apply_compat(${PROJECT_NAME})

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
