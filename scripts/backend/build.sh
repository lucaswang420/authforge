#!/bin/bash
set -e

BUILD_TYPE=Release
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_DIR="$( dirname "$( dirname "$SCRIPT_DIR" )" )"
BUILD_DIR="$PROJECT_DIR/build"

for arg in "$@"; do
    case $arg in
        Debug|Release|RelWithDebInfo|MinSizeRel)
            BUILD_TYPE=$arg
            ;;
        --debug)
            BUILD_TYPE=Debug
            ;;
    esac
done

echo "Building Project (Linux) - Config: $BUILD_TYPE"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_TESTS=ON
cmake --build . --config $BUILD_TYPE -- -j$(nproc)

cp "$PROJECT_DIR/OAuth2Server/config.json" "$BUILD_DIR/OAuth2Server/"
mkdir -p "$BUILD_DIR/OAuth2Server/test"
cp "$PROJECT_DIR/OAuth2Server/config.json" "$BUILD_DIR/OAuth2Server/test/"

echo "Build Complete!"
