cmake_minimum_required(VERSION 3.16)
project(OAuth2Server VERSION ${OAUTH2_PROJECT_VERSION} LANGUAGES CXX)

# Find Dependencies
find_package(Drogon CONFIG REQUIRED)
# libcurl: OAuth2Plugin is an OBJECT library, so the SMTP code in
# EmailService.cc is compiled into THIS executable; link libcurl here too.
find_package(CURL REQUIRED)

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
target_link_libraries(${PROJECT_NAME} PRIVATE OAuth2Plugin CURL::libcurl)

# Additional includes
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler compatibility (shared module)
oauth2_apply_compat(${PROJECT_NAME})

# Copy Swagger UI to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../docs/backend/api/swagger-ui"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/docs/api/swagger-ui"
)

# Enable testing
enable_testing()
add_subdirectory(test)
