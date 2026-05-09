cmake_minimum_required(VERSION 3.5)
project(OAuth2Test_test CXX)

# Collect source files
file(GLOB TEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
# set(TEST_SRC "test_main.cc" "MemoryStorageTest.cc" "PluginTest.cc" "ConfigTest.cc" "RedisStorageTest.cc" "PostgresStorageTest.cc" "AdvancedStorageTest.cc") # DEBUG: Isolate ConfigTest
file(GLOB PLUGIN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/*.cc")
file(GLOB STORAGE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../storage/*.cc")
file(GLOB SERVICE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../services/*.cc")
file(GLOB CTL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../controllers/*.cc")
file(GLOB FILTER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../filters/*.cc")
file(GLOB MODEL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../models/*.cc") # Uncomment if models exist
file(GLOB COMMON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../common/config/*.cc" "${CMAKE_CURRENT_SOURCE_DIR}/../common/error/*.cc" "${CMAKE_CURRENT_SOURCE_DIR}/../common/validation/*.cc" "${CMAKE_CURRENT_SOURCE_DIR}/../common/documentation/*.cc") # Common utilities

# Optional flag to restrict tests to memory-only (useful for environments without Postgres/Redis like macOS CI)
option(OAUTH2_MEMORY_TESTS_ONLY "Build only tests that do not require external databases" OFF)

# Explicitly list sources to avoid picking up broken UserTest.cc
set(TEST_SRC
    "test_main.cc"
    "MemoryStorageTest.cc"
    "PluginTest.cc"
    "ConfigTest.cc"
    "AdvancedStorageTest.cc"
    "RedirectUriValidationTest.cc"
    "PerformanceBenchmark.cc"
    "EnvConfigTest.cc"
    "RateLimiterTest.cc"
    "ConfigMigrationTest.cc"
    "common/ConfigManagerTest.cc"
    "common/ErrorHandlerTest.cc"
    "common/ValidatorTest.cc"
    "common/ValidationHelperTest.cc"
    "common/OpenApiGeneratorTest.cc"
    "common/OpenApiGeneratorParameterTypesTest.cc"
    "services/AuthServiceGetUserInfoTest.cc"
    "SecurityValidationTest.cc"
)

# Only add database-dependent tests if we are not restricted to memory-only
if(NOT OAUTH2_MEMORY_TESTS_ONLY)
    list(APPEND TEST_SRC
        "RedisStorageTest.cc"
        "PostgresStorageTest.cc"
        "SchemaSetup.cc"
        "UserTest.cc"
        "IntegrationE2ETest.cc"
        "OAuth2FlowE2ETest.cc"
    )
endif()

add_executable(${PROJECT_NAME} ${TEST_SRC} ${PLUGIN_SRC} ${STORAGE_SRC} ${SERVICE_SRC} ${MODEL_SRC} ${CTL_SRC} ${FILTER_SRC} ${COMMON_SRC})

drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/../views
                    ${CMAKE_CURRENT_BINARY_DIR})

# Include parent project directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/../storage
    ${CMAKE_CURRENT_SOURCE_DIR}/../services
    ${CMAKE_CURRENT_SOURCE_DIR}/../controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/../models
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)
# If using shared metrics or other libs, link them here too

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/../models/orm_compat.h")
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
        _CRT_SECURE_NO_WARNINGS
    )
elseif(APPLE)
    # macOS needs codecvt compatibility for C++17/20
    target_compile_options(${PROJECT_NAME} PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/../models/orm_compat.h")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    )
endif()

# ParseAndAddDrogonTests(${PROJECT_NAME})
add_test(NAME OAuth2Tests COMMAND ${PROJECT_NAME} WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# Copy config to the test executable directory for runtime access using generator expressions
# This replaces hardcoded configure_file paths to support multi-config generators (like MSVC)
if(MSVC OR XCODE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../config.json
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.json
        COMMENT "Copying config.json to test executable directory ($<CONFIG>)"
    )
else()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../config.json
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.json
        COMMENT "Copying config.json to test executable directory"
    )
endif()
