#!/bin/sh

# fetch git modules
git submodule update --init --recursive

# build math library
echo "================================================================================"
echo "= Building OpenLibM"
echo "================================================================================"
pushd openlibm
AR=../../../toolchain/llvm/bin/llvm-ar  CC=../../../toolchain/llvm/bin/clang LDFLAGS="--target=i386-pc-kush-elf --sysroot=/Users/tristan/kush/sysroot" CFLAGS="--target=i386-pc-kush-elf --sysroot=/Users/tristan/kush/sysroot" SFLAGS="--target=i386-pc-kush-elf --sysroot=/Users/tristan/kush/sysroot" make ARCH=i686 OS=kush DESTDIR=../../../sysroot libdir=/lib includedir=/usr/include install
popd
