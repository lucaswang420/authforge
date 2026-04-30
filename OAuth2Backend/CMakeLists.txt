cmake_minimum_required(VERSION 3.5)
project(OAuth2Server VERSION 1.0.0 LANGUAGES CXX)

include(CheckIncludeFileCXX)

# For macOS, force C++17 to avoid codecvt_utf8_utf16 compatibility issues
if(APPLE)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    message(STATUS "macOS detected: Forcing C++17 to avoid C++20 codecvt issues")
else()
    # For other platforms, use feature detection
    check_include_file_cxx(any HAS_ANY)
    check_include_file_cxx(string_view HAS_STRING_VIEW)
    check_include_file_cxx(coroutine HAS_COROUTINE)
    if (NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
        # Do nothing
    elseif (HAS_ANY AND HAS_STRING_VIEW)
        set(CMAKE_CXX_STANDARD 17)
    elseif (HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
        set(CMAKE_CXX_STANDARD 20)
    else ()
        set(CMAKE_CXX_STANDARD 14)
    endif ()
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} main.cc)

# Common utilities and API documentation
file(GLOB_RECURSE COMMON_SOURCES "common/*.cc" "common/*.h")
file(GLOB_RECURSE DOC_SOURCES "common/documentation/*.cc" "common/documentation/*.h")
list(APPEND SOURCES ${COMMON_SOURCES} ${DOC_SOURCES})

# ##############################################################################
# add_subdirectory(drogon) 
# target_link_libraries(${PROJECT_NAME} PRIVATE drogon)
#
# and comment out the following lines
find_package(Drogon CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)

# ##############################################################################

if (CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "c++17 or higher is required")
elseif (CMAKE_CXX_STANDARD LESS 20)
    message(STATUS "use c++17")
else ()
    message(STATUS "use c++20")
endif ()

aux_source_directory(controllers CTL_SRC)
aux_source_directory(services SERVICE_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
aux_source_directory(storage STORAGE_SRC)
aux_source_directory(models MODEL_SRC)

# Common utilities
file(GLOB_RECURSE COMMON_SOURCES "common/*.cc" "common/*.h")

drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
                    ${CMAKE_CURRENT_BINARY_DIR})
# use the following line to create views with namespaces.
# drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
#                     ${CMAKE_CURRENT_BINARY_DIR} TRUE)
# use the following line to create views with namespace CHANGE_ME prefixed
# and path namespaces.
# drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
#                     ${CMAKE_CURRENT_BINARY_DIR} TRUE CHANGE_ME)

target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                   ${CMAKE_CURRENT_SOURCE_DIR}/models
                                   ${CMAKE_CURRENT_SOURCE_DIR}/storage
                                   ${CMAKE_CURRENT_SOURCE_DIR}/services)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/models/orm_compat.h")
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
        _CRT_SECURE_NO_WARNINGS
    )
elseif(APPLE)
    # macOS needs codecvt compatibility for C++17/20
    target_compile_options(${PROJECT_NAME} PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/models/orm_compat.h")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    )
endif()

target_sources(${PROJECT_NAME}
               PRIVATE
               ${SRC_DIR}
               ${CTL_SRC}
               ${FILTER_SRC}
               ${PLUGIN_SRC}
               ${STORAGE_SRC}
               ${SERVICE_SRC}
               ${MODEL_SRC}
               ${COMMON_SOURCES})
# ##############################################################################
# uncomment the following line for dynamically loading views 
# set_property(TARGET ${PROJECT_NAME} PROPERTY ENABLE_EXPORTS ON)

# ##############################################################################


	# Copy Swagger UI files to build directory (cross-platform)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/docs/api/swagger-ui
	    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/docs/api/swagger-ui ${CMAKE_BINARY_DIR}/docs/api/swagger-ui
	    COMMAND ${CMAKE_COMMAND} -E echo "Swagger UI files copied successfully"
	    COMMENT "Copying Swagger UI files to build directory"
	)

enable_testing()
add_subdirectory(test)
# touch to force cmake rescan 2
