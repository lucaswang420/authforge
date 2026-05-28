# cmake/Compatibility.cmake
# Cross-compiler compatibility shims for Drogon ORM-generated code.
#
# Provides function oauth2_apply_compat(target) which:
#   - MSVC: /FI<orm_compat.h> + /utf-8 + _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#   - GCC/Clang: -include <orm_compat.h>
#
# The orm_compat.h path is relative to the repo root (OAuth2Plugin/include/oauth2/types/orm_compat.h).
#
# _Design: repo-structure-refactor §7.3_
# _Requirements: 9.1, 9.4_

function(oauth2_apply_compat target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "oauth2_apply_compat: target '${target}' does not exist")
    endif()

    set(_compat_header "${CMAKE_SOURCE_DIR}/OAuth2Plugin/include/oauth2/types/orm_compat.h")

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
