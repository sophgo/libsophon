#!/bin/bash

[ "$EUID" -eq 0 ] || sudo="sudo -E"

if [ -f /etc/lsb-release ]; then
    # Ubuntu
    $sudo apt update
    $sudo apt-get install -y \
            build-essential \
            git \
            cmake \
            ninja-build \
            pkg-config \
            libncurses5-dev \
            libgflags-dev \
            libgtest-dev

elif [ -f /etc/redhat-release ]; then
    # CentOS
    $sudo yum install -y centos-release-scl epel-release
    $sudo yum install -y \
            gcc \
            gcc-c++ \
            make \
            git \
            glibc-static \
            glibc-devel \
            libstdc++-static \
            libstdc++-devel \
            libstdc++ libgcc \
            glibc-static.i686 \
            glibc-devel.i686 \
            libstdc++-static.i686 \
            libstdc++.i686 \
            libgcc.i686 \
            ncurses-devel \
            gflags-devel \
            gtest-devel

else
    echo "Unknown Host"
fi
