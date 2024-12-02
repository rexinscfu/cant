#!/bin/bash

# Build configuration
BUILD_TYPE="Release"
BUILD_DIR="build"
INSTALL_DIR="install"
STM32_TARGET=1

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# Create build directory
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure CMake
echo -e "${GREEN}Configuring CMake...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_INSTALL_PREFIX=../$INSTALL_DIR \
    -DSTM32_TARGET=$STM32_TARGET \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
echo -e "${GREEN}Building...${NC}"
cmake --build . -- -j$(nproc)

# Run tests
echo -e "${GREEN}Running tests...${NC}"
ctest --output-on-failure

# Install
echo -e "${GREEN}Installing...${NC}"
cmake --install .

# Generate documentation
echo -e "${GREEN}Generating documentation...${NC}"
cmake --build . --target docs

# Check for warnings
WARNINGS=$(grep -i "warning:" compile_commands.json)
if [ ! -z "$WARNINGS" ]; then
    echo -e "${RED}Warnings found:${NC}"
    echo "$WARNINGS"
fi

cd .. 