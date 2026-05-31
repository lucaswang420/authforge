# cmake/Compatibility.cmake
# Cross-compiler compatibility shims for Drogon ORM-generated code.
#
# Provides function oauth2_apply_compat(target) which:
#   - MSVC: /FI<orm_compat.h> + /utf-8 + _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#   - GCC/Clang: -include <orm_compat.h>
#
# The orm_compat.h path is resolved relative to THIS module's own directory.
# IMPORTANT: capture CMAKE_CURRENT_LIST_DIR HERE (at include time) — it equals
# <repo>/cmake. Inside a function body CMAKE_CURRENT_LIST_DIR reflects the
# CALLER's listfile directory, not this module's, so it must NOT be read inside
# oauth2_apply_compat(). Capturing it now makes the compat-header path correct
# whether the module is included from the repo root or from a standalone
# `add_subdirectory(OAuth2Plugin)` consumer that put <repo>/cmake on its
# CMAKE_MODULE_PATH (PR #2 P2 fix).
#
# _Design: repo-structure-refactor §7.3_
# _Requirements: 9.1, 9.4_

set(OAUTH2_CMAKE_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL
    "Directory of the OAuth2 shared CMake modules")

function(oauth2_apply_compat target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "oauth2_apply_compat: target '${target}' does not exist")
    endif()

    set(_compat_header
        "${OAUTH2_CMAKE_MODULE_DIR}/../OAuth2Plugin/include/oauth2/types/orm_compat.h")

    if(MSVC)
        target_compile_options(${target} PRIVATE
            "/FI${_compat_header}"
            "/utf-8"
        )
        target_compile_definitions(${target} PRIVATE
            _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
        )
    else()
        target_compile_options(${target} PRIVATE
            "-include" "${_compat_header}"
        )
    endif()
endfunction()
