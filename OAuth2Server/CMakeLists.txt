cmake_minimum_required(VERSION 3.10)
project(OAuth2Server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Dependencies
find_package(Drogon CONFIG REQUIRED)

add_executable(${PROJECT_NAME} 
    main.cc
    AuthService.cc
    SchemaManager.cc
)

# Controllers, Filters, etc.
aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)

# Views
drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
                    ${CMAKE_CURRENT_BINARY_DIR})

target_sources(${PROJECT_NAME} PRIVATE 
    ${CTL_SRC} 
    ${FILTER_SRC}
)

# Link against the Plugin library (defined in root CMakeLists.txt)
target_link_libraries(${PROJECT_NAME} PRIVATE OAuth2Plugin)

# Additional includes
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler Specific Options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/../OAuth2Plugin/include/oauth2/orm_compat.h")
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    # Silence C++17 codecvt deprecation warning for Drogon ORM generated code
    target_compile_definitions(${PROJECT_NAME} PRIVATE _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/../OAuth2Plugin/include/oauth2/orm_compat.h")
endif()

# Enable testing
enable_testing()
add_subdirectory(test)
