# Implementation Plan: Fix macOS CI Failure (codecvt_utf8_utf16)

## Problem Description
The macOS CI is failing during the "Build" step with the following error:
`error: no member named 'codecvt_utf8_utf16' in namespace 'std'`
This occurs in the generated Drogon model files (e.g., `Oauth2AccessTokens.cc`).

## Root Cause
1. **Missing Include**: The generated model files use `std::codecvt_utf8_utf16` but do not explicitly include `<codecvt>`.
2. **Selective Force-Include**: The project has a compatibility header `orm_compat.h` designed to solve this, but it was only force-included for MSVC (Windows) in `CMakeLists.txt`.
3. **Platform Deprecation**: `std::codecvt` is deprecated in C++17 and potentially removed or hidden in newer macOS/Apple Clang SDKs.

## Solution
1. **Update CMake Configuration**:
   - Modify `OAuth2Plugin/CMakeLists.txt` to use the `-include` flag for Clang/GCC to force-include `orm_compat.h` in all translation units.
   - Modify `OAuth2Server/CMakeLists.txt` to do the same.
2. **Enhance Compatibility Header**:
   - Update `OAuth2Plugin/include/oauth2/orm_compat.h` to enable the fallback implementation of `std::codecvt_utf8_utf16` for macOS/Apple platforms, ensuring it works even if the SDK has removed it.

## Verification
- Once committed, the GitHub Actions macOS CI should pass the "Build" step.
- Windows and Linux CI should remain passing.
