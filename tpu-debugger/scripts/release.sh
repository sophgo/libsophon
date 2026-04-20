#!/bin/bash

# Get the absolute path of the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"


# Get the chip architecture
CHIP_ARCH=$1
TOOLCHAIN=$2
if [ -z "$CHIP_ARCH" ]; then
    echo "Error: Please specify the chip architecture as the first argument."
    exit 1
fi
if [ -z "$TOOLCHAIN" ] && [ "$CHIP_ARCH" == "cv184x" ]; then
    echo "Error: Please specify the toolchain as the second argument."
    exit 1
fi

echo "Starting release process..."

# Source the environment setup script to load necessary functions and variables
if [ -f "$SCRIPT_DIR/envsetup.sh" ]; then
    source "$SCRIPT_DIR/envsetup.sh" ${CHIP_ARCH}
else
    echo "Error: envsetup.sh not found in $SCRIPT_DIR"
    exit 1
fi

# Execute the rebuild function defined in envsetup.sh
echo "Running rebuild..."
if type rebuild &> /dev/null; then
    rebuild $TOOLCHAIN || { echo "Error: Rebuild failed"; exit 1; }
    pushd build > /dev/null; make install; popd > /dev/null || { echo "Error: Install failed"; exit 1; }
else
    echo "Error: 'rebuild' function not found in envsetup.sh"
    exit 1
fi

# Check if the install directory exists
INSTALL_DIR="$PROJECT_ROOT/install/"
if [ ! -d "$INSTALL_DIR" ]; then
    echo "Error: Build output directory $INSTALL_DIR does not exist."
    exit 1
fi

# Define the target path for releasing (can be overridden by environment variable)
# TARGET_PATH="${RELEASE_TARGET_PATH:-./release_output}"

# Create the target directory if it doesn't exist
# mkdir -p "$TARGET_PATH"

# Copy the built files from /install to the specific target path
# echo "Copying files from $INSTALL_DIR to $TARGET_PATH..."
# cp -r "$INSTALL_DIR"/* "$TARGET_PATH/"

echo "Release completed successfully. Files are available in $INSTALL_DIR"