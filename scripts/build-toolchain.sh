#!/usr/bin/env bash
set -e

# Versions
BINUTILS_VER=${BINUTILS_VER:-2.41}
GCC_VER=${GCC_VER:-13.2.0}

# Installation prefix and target triple
PREFIX=${PREFIX:-/opt/cross}
TARGET=${TARGET:-x86_64-elf}

# Directory to store sources and build artifacts
BUILD_DIR=${BUILD_DIR:-build-toolchain}

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Download sources if needed
if [ ! -d "binutils-$BINUTILS_VER" ]; then
    if [ ! -f "binutils-$BINUTILS_VER.tar.xz" ]; then
        wget "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.xz"
    fi
    tar -xf "binutils-$BINUTILS_VER.tar.xz"
fi

if [ ! -d "gcc-$GCC_VER" ]; then
    if [ ! -f "gcc-$GCC_VER.tar.xz" ]; then
        wget "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.xz"
    fi
    tar -xf "gcc-$GCC_VER.tar.xz"
fi

# Build binutils
mkdir -p build-binutils
cd build-binutils
"../binutils-$BINUTILS_VER/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror
make -j"$(nproc)"
make install
cd ..

# Build gcc
mkdir -p build-gcc
cd build-gcc
"../gcc-$GCC_VER/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c \
    --without-headers
make all-gcc -j"$(nproc)"
make all-target-libgcc -j"$(nproc)"
make install-gcc
make install-target-libgcc
cd ..

# Update PATH
export PATH="$PREFIX/bin:$PATH"
echo "Toolchain built in $PREFIX"

