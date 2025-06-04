#!/bin/sh

# Check if the path is provided as an argument
if [ "$#" -eq 1 ]; then
    LIBSOPHON_PATH="\$1"
else
    echo "Please provide the path to libsophon-0.4.9 as an argument."
    read -p "Enter the path: " LIBSOPHON_PATH
fi

# Unload the bmtpu.ko module
echo "Unloading bmtpu.ko module..."
sudo rmmod bmtpu

# Install the bmtpu.ko module
echo "Installing bmtpu.ko module..."
sudo insmod "$LIBSOPHON_PATH/bmtpu.ko"
sudo mdev -s

# Copy all files from lib to /lib
echo "Copying library files to /lib..."
sudo cp "$LIBSOPHON_PATH/lib/"* /lib/

# Add symbolic links
echo "Adding symbolic links..."
sudo ln -sf /lib/libbmlib.so.0 /lib/libbmlib.so
sudo ln -s /lib/libbmrt.so.1.0 /lib/libbmrt.so

# start bmcpu app
# sudo ./$LIBSOPHON_PATH/bin/test_bmcpu_app > bmcpu.log &

echo "Operation completed!"
